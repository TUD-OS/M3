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

use com::MemGate;
use errors::Error;
use rc::Rc;
use session::Pipe;
use vfs::Fd;
use vpe::VPE;

pub struct IndirectPipe {
    _pipe: Rc<Pipe>,
    rd_fd: Fd,
    wr_fd: Fd,
}

impl IndirectPipe {
    pub fn new(mem: &MemGate, mem_size: usize) -> Result<Self, Error> {
        let pipe = Rc::new(Pipe::new("pipe", mem, mem_size)?);
        Ok(IndirectPipe {
            rd_fd: VPE::cur().files().alloc(pipe.create_chan(true)?)?,
            wr_fd: VPE::cur().files().alloc(pipe.create_chan(false)?)?,
            _pipe: pipe,
        })
    }

    pub fn reader_fd(&self) -> Fd {
        self.rd_fd
    }

    pub fn close_reader(&self) {
        VPE::cur().files().remove(self.rd_fd);
    }

    pub fn writer_fd(&self) -> Fd {
        self.wr_fd
    }

    pub fn close_writer(&self) {
        VPE::cur().files().remove(self.wr_fd);
    }
}

impl Drop for IndirectPipe {
    fn drop(&mut self) {
        self.close_reader();
        self.close_writer();
    }
}
