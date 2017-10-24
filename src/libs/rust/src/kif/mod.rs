mod cap;
mod perm;
mod pedesc;

pub mod service;
pub mod syscalls;

pub use self::perm::*;
pub use self::pedesc::*;
pub use self::cap::*;

use cap::Selector;

pub const INVALID_SEL: Selector = 0xFFFF;
