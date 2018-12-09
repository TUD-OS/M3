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

//! Contains the serial struct

use arch;
use cell::RefCell;
use core::fmt;
use errors::Error;
use io;
use rc::Rc;

/// The serial line
pub struct Serial {
}

impl Serial {
    /// Creates a new serial line
    pub fn new() -> Rc<RefCell<Self>> {
        Rc::new(RefCell::new(Serial {}))
    }
}

impl io::Read for Serial {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, Error> {
        arch::serial::read(buf)
    }
}

impl io::Write for Serial {
    fn flush(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn write(&mut self, buf: &[u8]) -> Result<usize, Error> {
        arch::serial::write(buf)
    }
}

impl fmt::Debug for Serial {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Serial")
    }
}
