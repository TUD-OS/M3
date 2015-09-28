/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

namespace m3 {

/**
 * Synchronization primitives.
 */
class Sync {
public:
    /**
     * Prevents the compiler from reordering instructions. That is, the code-generator will put all
     * preceding load and store commands before load and store commands that follow this call.
     */
    static inline void compiler_barrier() {
        asm volatile ("" : : : "memory");
    }

    static inline void memory_barrier() {
#if defined(__t2__) or defined(__t3__)
        asm volatile ("memw" : : : "memory");
#else
        asm volatile ("mfence" : : : "memory");
#endif
    }

private:
    Sync();
};

}
