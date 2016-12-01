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

inline uint64_t CPU::read8b(uintptr_t) {
    // unused
    return 0;
}

inline void CPU::write8b(uintptr_t, uint64_t) {
    // unused
}

inline word_t CPU::get_sp() {
    word_t val;
    asm volatile (
          "mov.n %0, a1;"
          : "=a" (val)
    );
    return val;
}

inline word_t CPU::get_fp() {
    // unused
    return 0;
}

inline void CPU::jumpto(uintptr_t addr) {
    asm volatile (
        "jx    %0"
        :
        : "r"(addr)
    );
    UNREACHED;
}

inline void CPU::compute(cycles_t cycles) {
    word_t rem = cycles / 4;
    while(rem > 0)
        asm volatile ("addi.n %0, %0, -1" : "+r"(rem));
}

inline void CPU::memory_barrier() {
    asm volatile (
        "memw"
        :
        :
        : "memory"
    );
}

}
