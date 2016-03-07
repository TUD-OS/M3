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

#include <m3/Common.h>
#include <m3/Env.h>

typedef void (*start_func)();

extern void *_start;

EXTERN_C void try_run() {
    // is there something to run?
    start_func ptr = reinterpret_cast<start_func>(m3::env()->entry);
    if(ptr) {
        // remember exit location
        m3::env()->exit = reinterpret_cast<uintptr_t>(&_start);
        asm volatile (
            // tell crt0 that we're set the stackpointer
            "mov %2, %%rsp;"
            "jmp *%1;"
            : : "a"(0xDEADBEEF), "r"(ptr), "r"(m3::env()->sp)
        );
    }
}
