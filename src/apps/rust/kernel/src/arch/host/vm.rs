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

use base::dtu::EpId;
use base::errors::{Code, Error};
use base::goff;
use base::kif::{CapSel, PEDesc};
use base::GlobAddr;

use cap::MapFlags;
use pes::VPEDesc;

pub struct AddrSpace {
}

impl AddrSpace {
    pub fn new(_pe: &PEDesc) -> Result<Self, Error> {
        Ok(AddrSpace {})
    }

    pub fn new_with_pager(_pe: &PEDesc, _sep: EpId, _rep: EpId, _sgate: CapSel) -> Result<Self, Error> {
        Err(Error::new(Code::NotSup))
    }

    pub fn sep(&self) -> Option<EpId> {
        None
    }
    pub fn sgate_sel(&self) -> Option<CapSel> {
        None
    }

    pub fn setup(&self) {
    }

    pub fn map_pages(&self, _vpe: &VPEDesc, _virt: goff, _phys: GlobAddr,
                     _pages: usize, _attr: MapFlags) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }

    pub fn unmap_pages(&self, _vpe: &VPEDesc, _virt: goff, _pages: usize) -> Result<(), Error> {
        Err(Error::new(Code::NotSup))
    }
}
