//! Contains the modules for serial output, logging, etc.

pub mod log;
mod rdwr;
mod serial;

use arch;
pub use self::rdwr::{Read, Write, read_object};
pub use self::serial::Serial;

#[macro_export]
macro_rules! log_impl {
    ($type:expr, $($args:tt)*) => ({
        if $type {
            #[allow(unused_imports)]
            use $crate::io::Write;
            if let Some(l) = $crate::io::log::Log::get() {
                l.write_fmt(format_args!($($args)*)).unwrap();
            }
        }
    })
}

/// Macro for logging (includes a trailing newline)
///
/// The arguments are printed if `io::log::$type` is enabled.
///
/// # Examples
///
/// ```
/// log!(SYSC, "my log entry: {}, {}", 1, "test");
/// ```
#[macro_export]
macro_rules! log {
    ($type:tt, $fmt:expr)              => (log_impl!($crate::io::log::$type, concat!($fmt, "\n")));
    ($type:tt, $fmt:expr, $($arg:tt)*) => (log_impl!($crate::io::log::$type, concat!($fmt, "\n"), $($arg)*));
}

/// Initializes the I/O module
pub fn init() {
    arch::serial::init();
    log::init();
}

/// Reinitializes the I/O module (for VPE::run)
pub fn reinit() {
    log::reinit();
}
