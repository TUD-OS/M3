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

#![feature(asm)]
#![feature(const_fn)]
#![feature(core_intrinsics)]
#![feature(fnbox)]
#![feature(rustc_private)]
#![feature(trace_macros)]

#![no_std]

#[macro_use]
extern crate base;
#[macro_use]
extern crate bitflags;

// init stuff
#[cfg(target_os = "none")]
pub use arch::init::{env_run, exit};
#[cfg(target_os = "linux")]
pub use arch::init::{rust_init, rust_deinit};

#[macro_use]
pub mod io;
#[macro_use]
pub mod com;

pub use base::*;

pub mod cap;
pub mod server;
pub mod session;
pub mod syscalls;
pub mod vfs;
pub mod vpe;

mod arch;
