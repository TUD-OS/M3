#[cfg(target_os = "none")]
#[path = "gem5/mod.rs"]
mod inner;

#[cfg(target_os = "linux")]
#[path = "host/mod.rs"]
mod inner;

pub use self::inner::*;
