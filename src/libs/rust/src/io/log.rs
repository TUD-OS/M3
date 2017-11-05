use cell::RefCell;
use env;
use errors::Error;
use rc::Rc;
use io::Serial;
use vfs::{self, Write};

pub const SYSC: bool  = false;
pub const HEAP: bool  = false;
pub const FS: bool    = false;
pub const SERV: bool  = false;

const MAX_LINE_LEN: usize = 160;
const SUFFIX: &[u8] = b"\x1B[0m";

static mut LOG: Option<Log> = None;

pub struct Log {
    serial: Rc<RefCell<Serial>>,
    buf: [u8; MAX_LINE_LEN],
    pos: usize,
    start_pos: usize,
}

impl Log {
    pub fn get() -> &'static mut Log {
        unsafe { LOG.as_mut().unwrap() }
    }

    pub fn new() -> Self {
        Log {
            serial: Serial::get().clone(),
            buf: [0; MAX_LINE_LEN],
            pos: 0,
            start_pos: 0,
        }
    }

    fn write(&mut self, bytes: &[u8]) {
        for b in bytes {
            self.put_char(*b)
        }
    }

    fn put_char(&mut self, c: u8) {
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

            self.flush().unwrap();
        }
    }

    pub(crate) fn init(&mut self) {
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
}

impl vfs::Write for Log {
    fn flush(&mut self) -> Result<(), Error> {
        self.serial.borrow_mut().write(&self.buf[0..self.pos])?;
        self.pos = self.start_pos;
        Ok(())
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        self.write(buf);
        Ok(buf.len())
    }
}

pub fn init() {
    unsafe {
        LOG = Some(Log::new());
    }
    reinit();
}

pub fn reinit() {
    Log::get().init();
}
