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

#include <base/util/Math.h>
#include <base/Backtrace.h>
#include <base/Config.h>

namespace m3 {

const size_t Backtrace::CALL_INSTR_SIZE = 5;

size_t Backtrace::collect(uintptr_t *addr, size_t max) {
    uintptr_t bp;
    asm volatile ("mov %%rbp,%0" : "=a" (bp));

    uintptr_t stack = Math::round_dn<uintptr_t>(bp, STACK_SIZE);
    uintptr_t end = Math::round_up<uintptr_t>(bp, STACK_SIZE);
    uintptr_t start = end - STACK_SIZE;
    size_t i = 0;
    for(; i < max; ++i) {
        // prevent page-fault
        if(bp < start || bp >= end)
            break;

        bp = stack + (bp & (STACK_SIZE - 1));
        addr[i] = *(reinterpret_cast<uintptr_t*>(bp) + 1) - CALL_INSTR_SIZE;
        bp = *reinterpret_cast<uintptr_t*>(bp);
    }
    return i;
}

}
