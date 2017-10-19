pub mod cap;
pub mod perm;
pub mod pedesc;
pub mod service;
pub mod syscalls;

pub use self::perm::Perm;
pub use self::pedesc::PEDesc;
pub use self::cap::CapRngDesc;

use cap::Selector;

pub const INVALID_SEL: Selector = 0xFFFF;
