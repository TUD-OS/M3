pub mod log;
mod rdwr;
mod serial;

use arch;
pub use self::rdwr::{Read, Write, read_object};
pub use self::serial::Serial;

#[macro_export]
macro_rules! __log_impl {
    ($type:tt, $($args:tt)*) => ({
        if $crate::io::log::$type {
            #[allow(unused_imports)]
            use $crate::io::Write;
            $crate::io::log::Log::get().write_fmt(format_args!($($args)*)).unwrap();
        }
    })
}

#[macro_export]
macro_rules! log {
    ($type:tt, $fmt:expr)              => (__log_impl!($type, concat!($fmt, "\n")));
    ($type:tt, $fmt:expr, $($arg:tt)*) => (__log_impl!($type, concat!($fmt, "\n"), $($arg)*));
}

pub fn init() {
    arch::serial::init();
    log::init();
}

pub fn reinit() {
    log::reinit();
}
