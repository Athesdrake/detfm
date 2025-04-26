use anyhow::{bail, Context, Result};
use detfm::{
    detfm::pktnames::PacketNames,
    fmt::{DefaultFormatter, Formatter},
    rename::{PoolRenamer, Rename},
    renamer::Renamer,
    Detfm,
};
use rabc::{abc::ConstantPool, Abc, Movie, StreamWriter};
use std::fmt::Write;
use std::{mem::swap, time::Duration};
use time::JsInstant;
use unpacker::{MovieReader, Unpacker};
use wasm_bindgen::prelude::*;
use web_sys::js_sys::Function;

mod time;
mod utils;

#[wasm_bindgen(getter_with_clone)]
pub struct VersionInfo {
    pub detfm: String,
    pub detfm_wasm: String,
    pub unpacker: String,
}

#[wasm_bindgen]
#[derive(Clone, Copy, Default)]
pub enum Compression {
    Original,
    #[default]
    None,
    Zlib,
    Lzma,
}

impl From<Compression> for Option<rabc::Compression> {
    fn from(val: Compression) -> Self {
        Some(match val {
            Compression::Original => return None,
            Compression::None => rabc::Compression::None,
            Compression::Zlib => rabc::Compression::Zlib,
            Compression::Lzma => rabc::Compression::Lzma,
        })
    }
}

#[wasm_bindgen(inspectable)]
#[derive(Clone)]
pub struct DetfmOptions {
    /// Set the compression algorithm for the ouput file.
    pub compression: Compression,
    /// Change the server's ip to localhost server's port to the given value.
    pub proxy_port: Option<u16>,
    /// Callback for logging purpose. Arguments: `(level: String, message: String)`
    logger: Option<Function>,
}

#[wasm_bindgen]
impl DetfmOptions {
    #[wasm_bindgen(constructor)]
    #[must_use]
    pub fn new(
        compression: Option<Compression>,
        proxy_port: Option<u16>,
        logger: Option<Function>,
    ) -> Self {
        Self {
            compression: compression.unwrap_or_default(),
            proxy_port,
            logger,
        }
    }

    fn do_log(&self, level: &'static str, message: &str) {
        let null = JsValue::null();
        if let Some(logger) = &self.logger {
            if let Err(err) = logger.call2(&null, &level.into(), &message.into()) {
                web_sys::console::exception_2(&"Error while calling logger callback".into(), &err);
            }
        }
    }
    pub fn log(&self, message: &str) {
        web_sys::console::log_1(&message.into());
        self.do_log("log", message);
    }
    pub fn debug(&self, message: &str) {
        web_sys::console::debug_1(&message.into());
        self.do_log("debug", message);
    }
    pub fn info(&self, message: &str) {
        web_sys::console::info_1(&message.into());
        self.do_log("info", message);
    }
    pub fn warn(&self, message: &str) {
        web_sys::console::warn_1(&message.into());
        self.do_log("warn", message);
    }
    pub fn error(&self, message: &str) {
        web_sys::console::error_1(&message.into());
        self.do_log("error", message);
    }
}

#[wasm_bindgen]
#[must_use]
pub fn version() -> VersionInfo {
    VersionInfo {
        detfm: detfm::VERSION.to_owned(),
        detfm_wasm: env!("CARGO_PKG_VERSION").to_owned(),
        unpacker: unpacker::VERSION.to_owned(),
    }
}

#[wasm_bindgen]
#[must_use]
pub fn detfm(data: &[u8], opts: &DetfmOptions) -> Option<Vec<u8>> {
    utils::set_panic_hook();
    let mut timings = Vec::new();
    let boot = JsInstant::now();
    let result = match detfm_rs(data, opts, &mut timings, boot) {
        Ok(result) => Some(result),
        Err(err) => {
            opts.error(&err.to_string());
            None
        }
    };

    // Display stats
    display_stats(opts, timings, boot);
    result
}

fn detfm_rs(
    data: &[u8],
    opts: &DetfmOptions,
    timings: &mut Vec<(&str, Duration)>,
    boot: JsInstant,
) -> Result<Vec<u8>> {
    let fmt: Box<dyn Formatter> = Box::new(DefaultFormatter);

    opts.info(&format!("Reading file (length: {})", data.len()));
    let movie = Movie::from_buffer(data);
    timings.push(("Reading file", boot.elapsed()));
    let mut movie = movie.context("Error while reading the file")?;

    if movie.frame1().is_some() {
        opts.info("Unpacking.");
        movie = match unpack_movie(&movie) {
            Ok(movie) => movie,
            Err(err) => {
                opts.warn(&format!("Unpacking failed: {err}"));
                movie
            }
        };
        timings.push(("Unpacking", boot.elapsed()));
    }

    let (mut abc, mut cpool) = extract_abcfile(&mut movie)?;
    opts.info("Renaming invalid fields.");
    // Rename the symbols to something more readable
    // In fact it's the fully qualified name of a class,
    // so we could rename the symbol using the class' name, but that's not really useful
    for (id, name) in &movie.symbols {
        if !Renamer::invalid(name) {
            continue;
        }
        let pos = name.find('_').map_or(0, |n| n + 1);
        let mut new_name = format!("${}", &name[pos..]);
        if Renamer::invalid(&new_name) {
            new_name = fmt.symbols(*id);
        }
        cpool.replace_string(name, &new_name);
    }

    // Rename the first class as the Game class
    // Rename the symbol too, so we can Go to document class
    // Not using "Transformice" as the name, since this tool should work on other games too
    // NOTE: FrameLabelTag should be renamed too
    movie.symbols.entry(0).insert_entry("Game".to_owned());
    abc.classes[0].rename_str(&mut cpool, "Game").unwrap();

    timings.push(("Renaming invalid fields", boot.elapsed()));
    Renamer::new(fmt.as_ref())
        .rename_all(&mut abc, &mut cpool)
        .context("Error while renaming invalid symbols")?;

    opts.info("Analyzing methods and classes.");
    let mut detfm = Detfm::new(&mut abc, &mut cpool, fmt, PacketNames::default());
    detfm.simplify_init()?;

    let (classes, missing_classes) = detfm.analyze();
    if !missing_classes.is_empty() {
        let missing = missing_classes.join("\n - ");
        opts.warn(&format!("Some classes could not be found:\n - {missing}"));
    }
    timings.push(("Analyzing methods and classes", boot.elapsed()));

    opts.info("Unscrambling methods.");
    detfm.unscramble(&classes)?;

    timings.push(("Unscrambling methods", boot.elapsed()));
    opts.info("Renaming interesting stuff.");
    detfm.rename(&classes)?;
    timings.push(("Renaming", boot.elapsed()));
    // opts.info("Matching user-defined classes.");

    if let Some(port) = opts.proxy_port {
        if let Some((from, to)) = detfm.proxy2localhost(port) {
            opts.info(&format!("Proxying to {to:?} (was {from:?})."));
        } else {
            opts.warn("Server's ip not found.");
        }
        timings.push(("Proxying", boot.elapsed()));
    }

    opts.info("Writing file.");
    if let Some(compression) = opts.compression.into() {
        movie.compression = compression;
    }
    merge_abcfile(&mut movie, abc, cpool).unwrap();
    let mut stream = StreamWriter::new(Vec::with_capacity(movie.file_length as usize));

    movie.write(&mut stream).unwrap();
    Ok(stream.move_buffer())
}

fn unpack_movie(movie: &Movie) -> Result<Movie> {
    let mut writer = StreamWriter::default();
    let mut unpack = Unpacker::new(movie)?;

    if let Some(missing) = unpack.unpack(&mut writer)? {
        bail!("Unable to find binary with name: {}", missing);
    }
    Ok(Movie::from_buffer(writer.buffer())?)
}

fn extract_abcfile(movie: &mut Movie) -> Result<(Abc, ConstantPool)> {
    let Some(frame1) = movie.frame1_mut() else {
        bail!("Invalid SWF: Frame1 is not available.");
    };
    let mut abc = Abc::new();
    let mut cpool = ConstantPool::new();
    swap(&mut abc, &mut frame1.abcfile.abc);
    swap(&mut cpool, &mut frame1.abcfile.cpool);
    Ok((abc, cpool))
}
fn merge_abcfile(movie: &mut Movie, mut abc: Abc, mut cpool: ConstantPool) -> Result<()> {
    let Some(frame1) = movie.frame1_mut() else {
        bail!("Invalid SWF: Frame1 is not available.");
    };
    swap(&mut abc, &mut frame1.abcfile.abc);
    swap(&mut cpool, &mut frame1.abcfile.cpool);
    Ok(())
}

type TimingPoint<'a> = (&'a str, Duration);
fn display_stats(opts: &DetfmOptions, timings: Vec<TimingPoint>, boot: JsInstant) {
    let mut stats = "Timing stats\n".to_owned();
    let mut last = boot;
    for (name, point) in timings {
        let took = boot + point - last;
        if let Err(err) = writeln!(&mut stats, " - {name}: {took:.2?}") {
            opts.error(&format!("display_stats: {err}"));
        }
        last = boot + point;
    }
    if let Err(err) = writeln!(&mut stats, "Total: {:.2?}", boot.elapsed()) {
        opts.error(&format!("display_stats: {err}"));
    }
    opts.debug(&stats.to_string());
}
