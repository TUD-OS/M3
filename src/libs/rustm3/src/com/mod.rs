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

#[macro_use]
mod stream;

mod epmux;
mod gate;
mod mgate;
mod rgate;
mod sgate;

pub use self::epmux::EpMux;
pub use self::mgate::{MemGate, MGateArgs, Perm};
pub use self::rgate::{RecvGate, RGateArgs};
pub use self::sgate::{SendGate, SGateArgs};
pub use self::stream::*;

pub fn init() {
    rgate::init();
}

pub fn reinit() {
    epmux::EpMux::get().reset();
}
