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

use arch::env;
use cfg;
use errors::{Code, Error};
use kif::PEDesc;

#[repr(C, packed)]
#[derive(Copy, Clone, Debug)]
pub struct RBufSpace {
    pub cur: usize,
    pub end: usize,
}

impl RBufSpace {
    pub fn new() -> Self {
        Self::new_with(0, 0)
    }

    pub fn new_with(cur: usize, end: usize) -> Self {
        RBufSpace {
            cur: cur,
            end: end,
        }
    }

    pub fn get_std(&mut self, off: usize, _size: usize) -> usize {
        let pe = env::get().pe_desc();
        if pe.has_virtmem() {
            cfg::RECVBUF_SPACE + off
        }
        else {
            (pe.mem_size() - cfg::RECVBUF_SIZE_SPM) + off
        }
    }

    pub fn alloc(&mut self, pe: &PEDesc, size: usize) -> Result<usize, Error> {
        if self.end == 0 {
            let buf_sizes = cfg::SYSC_RBUF_SIZE + cfg::UPCALL_RBUF_SIZE + cfg::DEF_RBUF_SIZE;
            if pe.has_virtmem() {
                self.cur = cfg::RECVBUF_SPACE + buf_sizes;
                self.end = cfg::RECVBUF_SPACE + cfg::RECVBUF_SIZE;
            }
            else {
                self.cur = pe.mem_size() - cfg::RECVBUF_SIZE_SPM + buf_sizes;
                self.end = pe.mem_size();
            }
        }

        // TODO atm, the kernel allocates the complete receive buffer space
        let left = self.end - self.cur;
        if size > left {
            Err(Error::new(Code::NoSpace))
        }
        else {
            let res = self.cur;
            self.cur += size;
            Ok(res)
        }
    }

    pub fn free(&mut self, _addr: usize, _size: usize) {
        // TODO implement me
    }
}
