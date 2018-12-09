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

use base::dtu::PEId;
use base::errors::Error;
use base::GlobAddr;
use base::goff;
use core::fmt;
use mem::map::MemMap;

pub struct MemMod {
    gaddr: GlobAddr,
    size: usize,
    map: MemMap,
}

impl MemMod {
    pub fn new(pe: PEId, offset: goff, size: usize) -> Self {
        MemMod {
            gaddr: GlobAddr::new_with(pe, offset),
            size: size,
            map: MemMap::new(0, size),
        }
    }

    pub fn capacity(&self) -> usize {
        self.size
    }
    pub fn available(&self) -> usize {
        self.map.size().0
    }

    pub fn addr(&self) -> GlobAddr {
        self.gaddr
    }

    pub fn allocate(&mut self, size: usize, align: usize) -> Result<GlobAddr, Error> {
        self.map.allocate(size, align).map(|addr| self.gaddr + addr)
    }

    pub fn free(&mut self, addr: GlobAddr, size: usize) -> bool {
        if addr.pe() == self.gaddr.pe() {
            self.map.free(addr.offset() - self.gaddr.offset(), size);
            true
        }
        else {
            false
        }
    }
}

impl fmt::Debug for MemMod {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "MemMod[addr: {:?}, size: {} MiB, available: {} MiB, map: {:?}]",
            self.gaddr, self.capacity() / (1024 * 1024), self.available() / (1024 * 1024), self.map)
    }
}
