#[cfg(target_os = "none")]
#[path = "gem5/mod.rs"]
mod inner;

#[cfg(target_os = "linux")]
#[path = "host/mod.rs"]
mod inner;

#[cfg(target_arch = "x86_64")]
#[path = "x86_64/mod.rs"]
mod isa;

#[cfg(target_arch = "arm")]
#[path = "arm/mod.rs"]
mod isa;

pub use self::inner::*;
pub use self::isa::*;
