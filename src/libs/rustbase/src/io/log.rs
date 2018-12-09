/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

//! Contains the logger

use arch;
use cell::{StaticCell, RefCell};
use env;
use errors::Error;
use rc::Rc;
use io::{Serial, Write};

/// Default log message type
pub const DEF: bool     = true;
/// Logs system calls
pub const SYSC: bool    = false;
/// Logs heap operations
pub const HEAP: bool    = false;
/// Logs file system operations
pub const FS: bool      = false;
/// Logs server operations
pub const SERV: bool    = false;
/// Logs DTU operations (only on host)
pub const DTU: bool     = false;
/// Logs thread switching etc.
pub const THREAD: bool  = false;
/// Logs file multiplexing
pub const FILES: bool   = false;

const MAX_LINE_LEN: usize = 160;
const SUFFIX: &[u8] = b"\x1B[0m";

static LOG: StaticCell<Option<Log>> = StaticCell::new(None);

/// A buffered logger that writes to the serial line
pub struct Log {
    serial: Rc<RefCell<Serial>>,
    buf: [u8; MAX_LINE_LEN],
    pos: usize,
    start_pos: usize,
}

impl Log {
    /// Returns the logger
    pub fn get() -> Option<&'static mut Log> {
        LOG.get_mut().as_mut()
    }

    /// Creates a new logger
    pub fn new() -> Self {
        Log {
            serial: Serial::new(),
            buf: [0; MAX_LINE_LEN],
            pos: 0,
            start_pos: 0,
        }
    }

    fn write_bytes(&mut self, bytes: &[u8]) {
        for b in bytes {
            self.put_char(*b)
        }
    }

    fn put_char(&mut self, c: u8) {
        self.buf[self.pos] = c;
        self.pos += 1;

        if c == b'\n' || self.pos + SUFFIX.len() + 1 >= MAX_LINE_LEN {
            for i in 0..SUFFIX.len() {
                self.buf[self.pos] = SUFFIX[i];
                self.pos += 1;
            }
            if c != b'\n' {
                self.buf[self.pos] = b'\n';
                self.pos += 1;
            }

            self.flush().unwrap();
        }
    }

    pub(crate) fn init(&mut self) {
        let colors = ["31", "32", "33", "34", "35", "36"];
        let name = env::args().nth(0).unwrap_or("Unknown");
        let begin = match name.rfind('/') {
            Some(b) => b + 1,
            None    => 0,
        };

        self.pos = 0;
        let pe_id = arch::envdata::get().pe_id;
        self.write_fmt(format_args!(
            "\x1B[0;{}m[{:.8}@{:x}] ",
            colors[(pe_id as usize) % colors.len()],
            &name[begin..],
            pe_id
        )).unwrap();
        self.start_pos = self.pos;
    }
}

impl Write for Log {
    fn flush(&mut self) -> Result<(), Error> {
        self.serial.borrow_mut().write(&self.buf[0..self.pos])?;
        self.pos = self.start_pos;
        Ok(())
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        self.write_bytes(buf);
        Ok(buf.len())
    }
}

/// Initializes the logger
pub fn init() {
    LOG.set(Some(Log::new()));
    reinit();
}

/// Reinitializes the logger (for VPE::run)
pub fn reinit() {
    Log::get().unwrap().init();
}
