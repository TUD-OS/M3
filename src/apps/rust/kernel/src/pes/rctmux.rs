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

use base::goff;

pub const ENTRY_ADDR: goff = 0x1000;
pub const YIELD_ADDR: goff = 0x5FF0;
pub const FLAGS_ADDR: goff = 0x5FF8;

bitflags! {
    pub struct Flags : u64 {
        const STORE       = 1 << 0; // store operation required
        const RESTORE     = 1 << 1; // restore operation required
        const WAITING     = 1 << 2; // set by the kernel if a signal is required
        const SIGNAL      = 1 << 3; // used to signal completion to the kernel
    }
}
