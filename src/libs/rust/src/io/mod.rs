pub mod log;
mod serial;
mod std;

pub use self::serial::Serial;
pub use self::std::{stdin, stdout, stderr};

macro_rules! log_impl {
    ($type:tt, $($args:tt)*) => ({
        if $crate::io::log::$type {
            #[allow(unused_imports)]
            use $crate::vfs::Write;
            $crate::io::log::Log::get().write_fmt(format_args!($($args)*)).unwrap();
        }
    })
}

#[macro_export]
macro_rules! log {
    ($type:tt, $fmt:expr)              => (log_impl!($type, concat!($fmt, "\n")));
    ($type:tt, $fmt:expr, $($arg:tt)*) => (log_impl!($type, concat!($fmt, "\n"), $($arg)*));
}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => ({
        #[allow(unused_imports)]
        use $crate::vfs::Write;
        $crate::io::stdout().write_fmt(format_args!($($arg)*)).unwrap();
    });
}

#[macro_export]
macro_rules! println {
    ($fmt:expr)              => (print!(concat!($fmt, "\n")));
    ($fmt:expr, $($arg:tt)*) => (print!(concat!($fmt, "\n"), $($arg)*));
}

pub fn init() {
    serial::init();
    log::init();
    std::init();
}

pub fn reinit() {
    log::reinit();
}
