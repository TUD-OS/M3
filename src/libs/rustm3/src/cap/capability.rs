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

use core::ops;
use kif;
use syscalls;

pub type Selector = kif::CapSel;

bitflags! {
    pub struct CapFlags : u32 {
        const KEEP_CAP   = 0x1;
    }
}

// TODO isn't there a better way?
impl CapFlags {
    pub const fn const_empty() -> Self {
        CapFlags {
            bits: 0
        }
    }
}

#[derive(Debug)]
pub struct Capability {
    sel: Selector,
    flags: CapFlags,
}

impl Capability {
    pub const fn new(sel: Selector, flags: CapFlags) -> Self {
        Capability {
            sel: sel,
            flags: flags,
        }
    }

    pub fn sel(&self) -> Selector {
        self.sel
    }
    pub fn flags(&self) -> CapFlags {
        self.flags
    }
    pub fn set_flags(&mut self, flags: CapFlags) {
        self.flags = flags;
    }

    pub fn rebind(&mut self, sel: Selector) {
        self.release();
        self.sel = sel;
    }

    fn release(&mut self) {
        if (self.flags & CapFlags::KEEP_CAP).is_empty() {
            let crd = kif::CapRngDesc::new(kif::CapType::OBJECT, self.sel, 1);
            syscalls::revoke(0, crd, true).ok();
        }
    }
}

impl ops::Drop for Capability {
    fn drop(&mut self) {
        self.release();
    }
}
