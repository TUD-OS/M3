mod cap;
mod perm;
mod pedesc;

pub mod service;
pub mod syscalls;

pub use self::perm::*;
pub use self::pedesc::*;
pub use self::cap::*;

/// Represents an invalid capability selector
pub const INVALID_SEL: CapSel = 0xFFFF;
