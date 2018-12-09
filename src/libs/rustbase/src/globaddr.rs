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

use arch::dtu::PEId;
use core::fmt;
use core::ops;
use goff;

/// Represents a global address, which is a combination of a PE id and an offset within the PE.
///
/// If the PE supports virtual memory, the offset is a virtual address. Otherwise, it is a physical
/// address (the offset in the PE-internal memory).
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
#[repr(packed)]
pub struct GlobAddr {
    val: u64,
}

#[cfg(target_os = "none")]
const PE_SHIFT: u32 = 56;
#[cfg(target_os = "linux")]
const PE_SHIFT: u32 = 48;

impl GlobAddr {
    /// Creates a new global address from the given raw value
    pub fn new(addr: u64) -> GlobAddr {
        GlobAddr {
            val: addr
        }
    }
    /// Creates a new global address from the given PE id and offset
    pub fn new_with(pe: PEId, off: goff) -> GlobAddr {
        Self::new(((0x80 + pe as u64) << PE_SHIFT) | off)
    }

    /// Returns the raw value
    pub fn raw(&self) -> u64 {
        self.val
    }
    /// Returns the PE id
    pub fn pe(&self) -> PEId {
        ((self.val >> PE_SHIFT) - 0x80) as PEId
    }
    /// Returns the offset
    pub fn offset(&self) -> goff {
        (self.val & ((1 << PE_SHIFT) - 1)) as goff
    }
}

impl fmt::Debug for GlobAddr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "GlobAddr[pe: {}, off: {:#x}]", self.pe(), self.offset())
    }
}

impl ops::Add<goff> for GlobAddr {
    type Output = GlobAddr;

    fn add(self, rhs: goff) -> Self::Output {
        GlobAddr::new(self.val + rhs)
    }
}
