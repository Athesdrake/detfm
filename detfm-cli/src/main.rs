use anyhow::{anyhow, bail, Context, Result};
use clap::{Parser, ValueEnum};
use detfm::{
    detfm::pktnames::PacketNames,
    fmt::{DefaultFormatter, Formatter, LuaFormatter},
    rename::{PoolRenamer, Rename},
    renamer::Renamer,
    Detfm,
};
use rabc::{abc::ConstantPool, Abc, Movie, StreamWriter};
use std::{
    fmt::Display,
    fs::{self, File},
    io::Write,
    mem::swap,
    path::PathBuf,
    process::ExitCode,
    time::{Duration, Instant},
};
use unpacker::{MovieReader, Unpacker};

/// Unpack Transformice SWF file
#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    /// Increase output verbosity. Verbose messages go to stderr.
    #[arg(short = 'v', long, action = clap::ArgAction::Count, default_value_t = 0)]
    verbose: u8,

    /// Do not unpack the swf file before deobfuscating.
    #[arg(long = "no-unpack", action = clap::ArgAction::SetFalse)]
    unpack: bool,

    /// Specify a config file.
    #[arg(short = 'c', long = "config")]
    config: Option<PathBuf>,

    /// Set the compression algorithm for the ouput file.
    #[arg(short = 'C', long = "compression", default_value_t = Compression::None)]
    compression: Compression,

    /// The file url to deobfuscate. Can be a file from the filesystem or an url to download.
    #[arg(
        short = 'i',
        default_value = "https://www.transformice.com/Transformice.swf"
    )]
    input: String,

    /// Change the server's ip to localhost.
    #[arg(short = 'P', long, action = clap::ArgAction::SetTrue)]
    enable_proxy: bool,

    /// Change the server's port to the given value. Implies --enable-proxy
    #[arg(short = 'p', long)]
    proxy_port: Option<u16>,

    /// Path to a Lua script that let you customise how to format different names in the SWF
    #[arg(short = 'f', long = "format")]
    format_script: Option<String>,

    /// The outpout file.
    #[arg()]
    output: String,
}

#[derive(Debug, Clone, ValueEnum)]
enum Compression {
    Zlib,
    Lzma,
    None,
}

impl Args {
    fn verbosity(&self) -> log::Level {
        match self.verbose {
            0 => log::Level::Warn,
            1 => log::Level::Info,
            2 => log::Level::Debug,
            _ => log::Level::Trace,
        }
    }
}
impl Display for Compression {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let value = match self {
            Compression::Zlib => "zlib",
            Compression::Lzma => "lzma",
            Compression::None => "none",
        };
        write!(f, "{value}")
    }
}
impl From<Compression> for rabc::Compression {
    fn from(val: Compression) -> Self {
        match val {
            Compression::Zlib => rabc::Compression::Zlib,
            Compression::Lzma => rabc::Compression::Lzma,
            Compression::None => rabc::Compression::None,
        }
    }
}

fn main() -> ExitCode {
    let mut timings = Vec::new();
    let boot = Instant::now();

    let res = match impl_main(&mut timings, boot) {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            log::error!("{err}");
            ExitCode::FAILURE
        }
    };

    // Display stats
    display_stats(timings, boot);
    res
}

fn impl_main(timings: &mut Vec<(&str, Duration)>, boot: Instant) -> Result<()> {
    let args = Args::parse();
    stderrlog::new()
        .module(module_path!())
        .verbosity(args.verbosity())
        .init()
        .unwrap();

    let fmt: Box<dyn Formatter> = match &args.format_script {
        Some(file) => {
            load_formatter_script(file).context(format!("Error while reading file {file:?}"))?
        }
        None => Box::new(DefaultFormatter),
    };
    let packet_names = match args.config {
        Some(config) if config.is_file() => {
            load_config(config).context("Error while loading config")?
        }
        Some(config) => {
            log::warn!("Invalid config file: {config:?}");
            None
        }
        _ => None,
    };

    log::info!("Reading file {}", args.input);
    let movie = Movie::from_file(&args.input);
    timings.push(("Reading file", boot.elapsed()));
    let mut movie = movie.context(format!("Error while reading the file {0:?}", args.input))?;

    if args.unpack && movie.frame1().is_some() {
        log::info!("Unpacking.");
        let res = unpack_movie(&movie);
        timings.push(("Unpacking", boot.elapsed()));
        movie = res.context("Unpacking failed")?;
    }

    let (mut abc, mut cpool) = extract_abcfile(&mut movie)?;
    log::info!("Renaming invalid fields.");
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

    log::info!("Analyzing methods and classes.");
    let packet_names = packet_names.unwrap_or_default();
    let mut detfm = Detfm::new(&mut abc, &mut cpool, fmt, packet_names);
    detfm.simplify_init()?;

    let (classes, missing_classes) = detfm.analyze();
    if !missing_classes.is_empty() {
        let missing = missing_classes.join("\n - ");
        log::warn!("Some classes could not be found:\n - {missing}");
    }
    timings.push(("Analyzing methods and classes", boot.elapsed()));

    log::info!("Unscrambling methods.");
    detfm.unscramble(&classes)?;

    timings.push(("Unscrambling methods", boot.elapsed()));
    log::info!("Renaming interesting stuff.");
    detfm.rename(&classes)?;
    timings.push(("Renaming", boot.elapsed()));
    // log::info!("Matching user-defined classes.");

    let default_port = args.enable_proxy.then_some(11801);
    if let Some(port) = args.proxy_port.or(default_port) {
        if let Some((from, to)) = detfm.proxy2localhost(port) {
            log::info!("Proxying to {to:?} (was {from:?}).");
        } else {
            log::warn!("Server's ip not found.");
        }
        timings.push(("Proxying", boot.elapsed()));
    }

    log::info!("Writing file.");
    // disable compression by default to speed up the write routine
    movie.compression = args.compression.into();
    merge_abcfile(&mut movie, abc, cpool).unwrap();
    let mut stream = StreamWriter::new(Vec::with_capacity(movie.file_length as usize));
    let mut file = File::create(args.output).unwrap();

    movie.write(&mut stream).unwrap();
    file.write_all(stream.buffer()).unwrap();
    Ok(())
}

fn load_formatter_script(file: &String) -> Result<Box<dyn Formatter>> {
    match LuaFormatter::from_script(fs::read_to_string(file)?) {
        Ok(fmt) => Ok(Box::new(fmt)),
        Err(err) => Err(anyhow!("{err}")),
    }
}

fn load_config(config_path: PathBuf) -> Result<Option<PacketNames>> {
    let config = jzon::parse(&fs::read_to_string(config_path)?)?;
    let mut packet_names = None;
    if let Some(names) = config.get("packet_names") {
        packet_names = Some(PacketNames::from_json(names)?);
    }
    Ok(packet_names)
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
fn display_stats(timings: Vec<TimingPoint>, boot: Instant) {
    if log::log_enabled!(log::Level::Debug) {
        log::debug!("Timing stats:");
        let mut last = boot;
        for (name, point) in timings {
            let took = boot + point - last;
            log::debug!(" - {name}: {took:.2?}");
            last = boot + point;
        }
        log::debug!("Total: {:.2?}", boot.elapsed());
    }
}
