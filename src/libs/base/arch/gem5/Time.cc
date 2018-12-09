/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/util/Time.h>
#include <base/DTU.h>

static inline cycles_t gem5_debug(unsigned msg) {
#if defined(__x86_64__)
    cycles_t res;
    asm volatile (
        ".byte 0x0F, 0x04;"
        ".word 0x63;"
        : "=a"(res) : "D"(msg)
    );
    return res;
#else
    register cycles_t r0 asm ("r0") = msg;
    asm volatile (
        ".long 0xEE630110"
        : "+r"(r0)
    );
    return r0;
#endif
}

namespace m3 {

cycles_t Time::start(unsigned msg) {
    CPU::compiler_barrier();
    return gem5_debug(START_TSC | msg);
}

cycles_t Time::stop(unsigned msg) {
    cycles_t res = gem5_debug(STOP_TSC | msg);
    CPU::compiler_barrier();
    return res;
}

}
