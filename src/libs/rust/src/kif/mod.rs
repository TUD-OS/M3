pub mod syscalls;
pub mod cap;
pub mod perm;

pub use self::perm::Perm;

use cap::Selector;

pub const INVALID_SEL: Selector = 0xFFFF;
