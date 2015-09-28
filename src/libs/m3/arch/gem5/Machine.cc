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

#include <m3/Machine.h>

#include <stdlib.h>
#include <unistd.h>

namespace m3 {

void Machine::shutdown() {
    asm volatile (
        "mfence;"
        "mov 0(%1, %0, 1), %%rax"
        : : "r"(0x21 << 8), "r"(0xFFFF0000), "D"(0) : "rax"
    );
    while(1)
        asm volatile ("hlt");
}

int Machine::write(const char *str, size_t len) {
    static const char *fileAddr = "stdout";
    asm volatile (
        "mfence;"
        "mov 0(%1, %0, 1), %%rax"
        : : "r"(0x4f << 8), "r"(0xFFFF0000), "D"(str), "S"(len), "d"(0), "c"(fileAddr) : "rax"
    );
    return 0;
}

ssize_t Machine::read(char *dst, size_t max) {
    ssize_t res;
    asm volatile (
        "mfence;"
        "mov 0(%2, %1, 1), %%rax"
        : "=a"(res) : "r"(0x50 << 8), "r"(0xFFFF0000), "D"(dst), "S"(max), "d"(0)
    );
    return res;
}

}
