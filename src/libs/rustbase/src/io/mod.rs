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
            $crate::io::log::Log::get().write_fmt(format_args!($($args)*)).unwrap();
        }
    })
}

#[macro_export]
macro_rules! log {
    ($type:tt, $fmt:expr)              => (log_impl!($crate::io::log::$type, concat!($fmt, "\n")));
    ($type:tt, $fmt:expr, $($arg:tt)*) => (log_impl!($crate::io::log::$type, concat!($fmt, "\n"), $($arg)*));
}

pub fn init() {
    arch::serial::init();
    log::init();
}

pub fn reinit() {
    log::reinit();
}
