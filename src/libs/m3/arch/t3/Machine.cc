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

#include <m3/stream/Serial.h>
#include <m3/DTU.h>
#include <m3/Machine.h>
#include <cstring>

namespace m3 {

void Machine::shutdown() {
    register int a2 __asm__ ("a2") = 1;
    register int a3 __asm__ ("a3") = 0;
    asm volatile (
        "simcall" : : "a"(a2), "a"(a3)
    );
    UNREACHED;
}

int Machine::write(const char *str, size_t len) {
    register int a2 __asm__ ("a2") = 4;
    register int a3 __asm__ ("a3") = 1;
    register const char *a4 __asm__ ("a4") = str;
    register size_t a5 __asm__ ("a5") = len;
    register int ret_val __asm__ ("a2");
    register int ret_err __asm__ ("a3");
    asm volatile ("simcall"
            : "=a" (ret_val), "=a" (ret_err)
            : "a" (a2), "a" (a3), "a" (a4), "a" (a5));
    return ret_err;
}

ssize_t Machine::read(char *buf, size_t len) {
    register int a2 __asm__ ("a2") = 3;
    register int a3 __asm__ ("a3") = 0;
    register const char *a4 __asm__ ("a4") = buf;
    register size_t a5 __asm__ ("a5") = len;
    register int ret_val __asm__ ("a2");
    register int ret_err __asm__ ("a3");
    asm volatile ("simcall"
            : "=a" (ret_val), "=a" (ret_err)
            : "a" (a2), "a" (a3), "a" (a4), "a" (a5));
    return ret_err != 0 ? -ret_err : ret_val;
}

}
