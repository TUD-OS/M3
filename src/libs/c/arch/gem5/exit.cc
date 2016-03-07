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
#include <cstdlib>

EXTERN_C NORETURN void _exit(int) {
    uintptr_t jmpaddr = m3::env()->exit;
    if(jmpaddr != 0) {
        m3::env()->entry = 0;
        asm volatile ("jmp *%0" : : "r"(jmpaddr));
    }

    while(1)
        asm volatile ("hlt");
}
