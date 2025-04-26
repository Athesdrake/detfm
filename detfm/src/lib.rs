pub mod detfm;
pub mod fmt;
pub mod rename;
pub mod renamer;
pub use detfm::Detfm;

#[allow(dead_code)]
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
