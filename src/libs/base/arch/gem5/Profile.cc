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
#include <sys/time.h>

namespace m3 {

static cycles_t rdtsc() {
    uint32_t u, l;
    asm volatile ("rdtsc" : "=a" (l), "=d" (u) : : "memory");
    return (cycles_t)u << 32 | l;
}

cycles_t Profile::start(unsigned) {
    Sync::memory_barrier();
    return rdtsc();
}

cycles_t Profile::stop(unsigned) {
    cycles_t res = rdtsc();
    Sync::memory_barrier();
    return res;
}

}
