mod serial;
mod std;

pub use base::io::*;
pub use self::serial::*;
pub use self::std::{stdin, stdout, stderr};

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => ({
        #[allow(unused_imports)]
        use $crate::io::Write;
        $crate::io::stdout().write_fmt(format_args!($($arg)*)).unwrap();
    });
}

#[macro_export]
macro_rules! println {
    ($fmt:expr)              => (print!(concat!($fmt, "\n")));
    ($fmt:expr, $($arg:tt)*) => (print!(concat!($fmt, "\n"), $($arg)*));
}

pub fn init() {
    ::base::io::init();
    std::init();
}

pub fn reinit() {
    ::base::io::reinit();
}
