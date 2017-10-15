use core::fmt;
use libc;

// TODO how to move the logging stuff to somewhere else?
pub const SYSC: bool  = false;
pub const HEAP: bool  = false;
pub const FS: bool    = false;

#[macro_export]
macro_rules! log {
    ($type:tt, $($arg:tt)*) => ({
        if $crate::io::$type {
            println!($($arg)*);
        }
    })
}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => ({
        $crate::io::print_fmt(format_args!($($arg)*));
    });
}

#[macro_export]
macro_rules! println {
    ($fmt:expr) => (print!(concat!($fmt, "\n")));
    ($fmt:expr, $($arg:tt)*) => (print!(concat!($fmt, "\n"), $($arg)*));
}

pub struct Serial {
}

impl Serial {
    pub fn put_str(&self, s: &str) {
        unsafe {
            libc::gem5_writefile(s.as_ptr(), s.len() as u64, 0, "stdout\0".as_ptr() as u64);
        }
    }
}

impl fmt::Write for Serial {
    fn write_str(&mut self, s: &str) -> ::core::fmt::Result {
        self.put_str(s);
        Ok(())
    }
}

pub fn print_fmt(args: fmt::Arguments) {
    use core::fmt::Write;
    let mut ser = Serial {};
    ser.write_fmt(args).unwrap();
}
