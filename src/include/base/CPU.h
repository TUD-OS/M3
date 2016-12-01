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

namespace m3 {

class CPU {
public:
    static inline uint64_t read8b(uintptr_t addr);
    static inline void write8b(uintptr_t addr, uint64_t val);

    static inline word_t get_sp();

    NORETURN static inline void jumpto(uintptr_t addr);

    static inline void compute(cycles_t cycles);

    /**
     * Prevents the compiler from reordering instructions. That is, the code-generator will put all
     * preceding load and store commands before load and store commands that follow this call.
     */
    static inline void compiler_barrier() {
        asm volatile ("" : : : "memory");
    }

    static inline void memory_barrier();
};

}

#if defined(__x86_64__)
#   include <base/arch/x86_64/CPU.h>
#elif defined(__arm__)
#   include <base/arch/arm/CPU.h>
#endif
