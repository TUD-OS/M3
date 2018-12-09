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

use cap::Selector;
use col::Vec;
use com::VecSink;
use errors::{Code, Error};
use goff;
use io;
use kif;
use session::Pager;
use vfs;

impl vfs::Seek for io::Serial {
    fn seek(&mut self, _off: usize, _whence: vfs::SeekMode) -> Result<usize, Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl vfs::Map for io::Serial {
    fn map(&self, _pager: &Pager, _virt: goff,
           _off: usize, _len: usize, _prot: kif::Perm) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }
}

impl vfs::File for io::Serial {
    fn fd(&self) -> vfs::Fd {
        0
    }
    fn set_fd(&mut self, _fd: vfs::Fd) {
    }

    fn evict(&mut self) {
    }

    fn close(&mut self) {
    }

    fn stat(&self) -> Result<vfs::FileInfo, Error> {
        Err(Error::new(Code::NotSup))
    }

    fn file_type(&self) -> u8 {
        b'S'
    }

    fn exchange_caps(&self, _vpe: Selector,
                            _dels: &mut Vec<Selector>,
                            _max_sel: &mut Selector) -> Result<(), Error> {
        // nothing to do
        Ok(())
    }

    fn serialize(&self, _s: &mut VecSink) {
        // nothing to do
    }
}
