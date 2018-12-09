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

pub fn read8b(addr: usize) -> u64 {
    let res: u64;
    unsafe {
        asm!(
            "ldrd $0, [$1]"
            : "=r"(res)
            : "r"(addr)
            : : "volatile"
        );
    }
    res
}

pub fn write8b(addr: usize, val: u64) {
    unsafe {
        asm!(
            "strd $0, [$1]"
            : : "r"(val), "r"(addr)
            : : "volatile"
        );
    }
}

pub fn get_sp() -> usize {
    let res: usize;
    unsafe {
        asm!(
            "mov $0, r13;"
            : "=r"(res)
        );
    }
    res
}

pub fn get_bp() -> usize {
    let val: usize;
    unsafe {
        asm!(
            "mov $0, r11;"
            : "=r"(val)
        );
    }
    val
}

pub fn jmp_to(addr: usize) {
    unsafe {
        asm!(
            "mov pc, $0;"
            : : "r"(addr)
            : : "volatile"
        );
    }
}

pub fn gem5_debug(msg: usize) -> time::Time {
    let mut res = msg as time::Time;
    unsafe {
        asm!(
            ".long 0xEE630110"
            : "+{r0}"(res)
        );
    }
    res
}
