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

#include <base/util/Profile.h>
#include <base/DTU.h>

namespace m3 {

static cycles_t rdtsc() {
    uint32_t u, l;
    asm volatile ("rdtsc" : "=a" (l), "=d" (u) : : "memory");
    return (cycles_t)u << 32 | l;
}

cycles_t Profile::start(unsigned msg) {
    Sync::compiler_barrier();
    DTU::get().debug_msg(START_TSC | msg);
    return rdtsc();
}

cycles_t Profile::stop(unsigned msg) {
    DTU::get().debug_msg(STOP_TSC | msg);
    cycles_t res = rdtsc();
    Sync::compiler_barrier();
    return res;
}

}
