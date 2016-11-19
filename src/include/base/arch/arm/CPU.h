/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#pragma once

#include <base/Common.h>
#include <base/CPU.h>

namespace m3 {

inline uint64_t CPU::read8b(uintptr_t addr) {
    uint64_t res;
    asm volatile (
        "ldrd %0, [%1]"
        : "=r"(res)
        : "r"(addr)
    );
    return res;
}

inline void CPU::write8b(uintptr_t addr, uint64_t val) {
    asm volatile (
        "strd %0, [%1]"
        : : "r"(val), "r"(addr)
    );
}

inline word_t CPU::get_sp() {
    word_t val;
    asm volatile (
        "mov %0, r13;"
        : "=r" (val)
    );
    return val;
}

inline void CPU::jumpto(uintptr_t addr) {
    asm volatile (
        "blx %0"
        :
        : "r"(addr)
    );
    UNREACHED;
}

inline void CPU::memory_barrier() {
    asm volatile (
        "dmb"
        :
        :
        : "memory"
    );
}

}
