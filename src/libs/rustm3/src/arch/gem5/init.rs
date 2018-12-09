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

use arch;
use com;
use cpu;
use heap;
use io;
use syscalls;
use vfs;
use vpe;

#[no_mangle]
pub extern "C" fn exit(code: i32) {
    io::deinit();
    vfs::deinit();
    syscalls::exit(code);
    cpu::jmp_to(arch::env::get().exit_addr());
}

extern "C" {
    fn main() -> i32;
}

#[no_mangle]
pub extern "C" fn env_run() {
    let res = if arch::env::get().has_lambda() {
        io::reinit();
        com::reinit();
        vpe::reinit();
        arch::env::closure().call()
    }
    else {
        heap::init();
        vpe::init();
        io::init();
        com::init();
        unsafe { main() }
    };
    exit(res)
}
