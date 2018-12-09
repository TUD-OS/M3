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
use heap;
use io;
use libc;
use syscalls;
use vfs;
use vpe;

fn rust_exit(code: i32) {
    io::deinit();
    vfs::deinit();
    syscalls::exit(code);
    arch::dtu::deinit();
}

#[no_mangle]
pub extern "C" fn rust_init(argc: i32, argv: *const *const i8) {
    extern "C" {
        fn dummy_func();
    }

    // ensure that host's init library gets linked in
    unsafe {
        dummy_func();
    }

    heap::init();
    arch::env::init(argc, argv);
    vpe::init();
    com::init();
    io::init();
    arch::dtu::init();
}

#[no_mangle]
pub extern "C" fn rust_deinit(status: i32, _arg: *const libc::c_void) {
    rust_exit(status);
}
