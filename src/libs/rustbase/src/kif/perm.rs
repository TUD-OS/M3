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

bitflags! {
    /// The permission bitmap that is used for memory and mapping capabilities.
    pub struct Perm : u8 {
        /// Read permission
        const R = 1;
        /// Write permission
        const W = 2;
        /// Execute permission
        const X = 4;
        /// Read + write permission
        const RW = Self::R.bits | Self::W.bits;
        /// Read + write + execute permission
        const RWX = Self::R.bits | Self::W.bits | Self::X.bits;
    }
}
