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

use time;

fn rdtsc() -> time::Time {
    let u: u32;
    let l: u32;
    unsafe {
        asm!(
            "rdtsc"
            : "={rax}"(l), "={rdx}"(u)
        );
    }
    (u as time::Time) << 32 | (l as time::Time)
}

pub fn start(_msg: usize) -> time::Time {
    rdtsc()
}

pub fn stop(_msg: usize) -> time::Time {
    rdtsc()
}
