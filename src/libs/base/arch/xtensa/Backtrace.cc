/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Backtrace.h>
#include <base/Config.h>
#include <xtensa/hal.h>

/*
 * Inspired from Linux's code for backtraces on Xtensa
 */

namespace m3 {

static uintptr_t addr_from_ra(word_t ra, word_t sp) {
    return (ra & 0x3fffffff) | (sp & 0xc0000000);
}

static void get_regs(word_t *a0, word_t *a1) {
    register word_t a2 __asm__ ("a2");
    register word_t a3 __asm__ ("a3");
    asm volatile (
        "mov  %0, a0;"
        "mov  %1, a1;"
        : "=a"(a2), "=a"(a3)
    );
    *a0 = a2;
    *a1 = a3;
}

size_t Backtrace::collect(uintptr_t *addr, size_t max) {
    uintptr_t pc = reinterpret_cast<uintptr_t>(&Backtrace::print);
    word_t *psp;
    word_t sp_start, sp_end;
    word_t a0, a1;
    get_regs(&a0, &a1);

    sp_start = a1 & ~(STACK_SIZE - 1);
    sp_end = sp_start + STACK_SIZE;

    /* Spill the register window to the stack first. */
    xthal_window_spill();

    /* Read the stack frames one by one and create the PC
     * from the a0 and a1 registers saved there.
     */
    size_t i = 0;
    for(; i < max && a1 > sp_start && a1 < sp_end; ++i) {
        pc = addr_from_ra(a0, pc);

        addr[i] = pc - 3;

        psp = reinterpret_cast<word_t*>(a1);
        a0 = *(psp - 4);
        a1 = *(psp - 3);
        if(!a0 || a1 <= reinterpret_cast<word_t>(psp))
            break;
    }
    return i;
}

}
