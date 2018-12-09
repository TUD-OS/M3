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

#include <sys/time.h>

namespace m3 {

cycles_t Time::start(unsigned u) {
    return stop(u);
}

cycles_t Time::stop(unsigned) {
#if defined(__i386__) or defined(__x86_64__)
    uint32_t u, l;
    asm volatile ("rdtsc" : "=a" (l), "=d" (u) : : "memory");
    return static_cast<cycles_t>(u) << 32 | l;
#elif defined(__arm__)
    struct timeval tv;
    gettimeofday(&tv,nullptr);
    return static_cast<cycles_t>(tv.tv_sec) * 1000000 + static_cast<cycles_t>(tv.tv_usec);
#else
#   warning "Cycle counter not supported"
    return 0;
#endif
}

}
