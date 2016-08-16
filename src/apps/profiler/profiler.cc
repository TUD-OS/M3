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

#include <base/Config.h>
#include <base/util/Sync.h>

static void writeback_line(uintptr_t addr) {
    asm volatile (
        "dhwb   %0, 0;"
        : : "a"(addr)
    );
}

static inline uint32_t get_cycles() {
    uint32_t val;
    asm volatile (
          "rsr    %0, CCOUNT;"
          : "=a" (val)
    );
    return val;
}

int main() {
    volatile uint32_t *addr = reinterpret_cast<volatile uint32_t*>(DRAM_CCOUNT);
    addr[0] = 0;
    addr[1] = 0;
    uint32_t last = 0;
    while(1) {
        uint32_t now = get_cycles();
        addr[0] = now;
        if(now < last)
            addr[1]++;
        last = now;
        m3::Sync::memory_barrier();
        writeback_line(reinterpret_cast<uintptr_t>(addr));
    }
    return 0;
}
