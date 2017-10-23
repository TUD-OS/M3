use core::fmt;
use core::fmt::Write;
use dtu;
use env;
use libc;

// TODO how to move the logging stuff to somewhere else?
pub const SYSC: bool  = false;
pub const HEAP: bool  = false;
pub const FS: bool    = false;
pub const SERV: bool  = false;

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

const MAX_LINE_LEN: usize = 160;
const SUFFIX: &[u8] = b"\x1B[0m";

pub struct Serial {
    buf: [u8; MAX_LINE_LEN],
    pos: usize,
    start_pos: usize,
}

static mut SERIAL: Serial = Serial::new();

impl Serial {
    fn get() -> &'static mut Self {
        unsafe { &mut SERIAL }
    }

    const fn new() -> Self {
        Serial {
            buf: [0; MAX_LINE_LEN],
            pos: 0,
            start_pos: 0,
        }
    }

    fn init(&mut self) {
        let env = env::data();
        let colors = ["31", "32", "33", "34", "35", "36"];
        let name = env::args().nth(0).unwrap_or("Unknown");
        let begin = match name.rfind('/') {
            Some(b) => b + 1,
            None    => 0,
        };

        self.pos = 0;
        self.write_fmt(format_args!(
            "\x1B[0;{}m[{:.8}@{:x}] ",
            colors[(env.pe as usize) % colors.len()],
            &name[begin..],
            env.pe
        )).unwrap();
        self.start_pos = self.pos;
    }

    pub fn put_str(&mut self, s: &str) {
        for c in s.as_bytes() {
            self.put_char(*c);
        }
    }

    pub fn put_char(&mut self, c: u8) {
        self.buf[self.pos] = c;
        self.pos += 1;

        if c == '\n' as u8 || self.pos + SUFFIX.len() + 1 >= MAX_LINE_LEN {
            for i in 0..SUFFIX.len() {
                self.buf[self.pos] = SUFFIX[i];
                self.pos += 1;
            }
            if c != '\n' as u8 {
                self.buf[self.pos] = '\n' as u8;
                self.pos += 1;
            }
            self.flush();
        }
    }

    pub fn flush(&mut self) {
        let s = &self.buf[0..self.pos];
        dtu::DTU::print(s);
        unsafe {
            libc::gem5_writefile(s.as_ptr(), s.len() as u64, 0, "stdout\0".as_ptr() as u64);
        }
        self.pos = self.start_pos;
    }
}

impl fmt::Write for Serial {
    fn write_str(&mut self, s: &str) -> ::core::fmt::Result {
        self.put_str(s);
        Ok(())
    }
}

pub fn print_fmt(args: fmt::Arguments) {
    Serial::get().write_fmt(args).unwrap();
}

pub fn init() {
    Serial::get().init();
}
