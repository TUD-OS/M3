/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Copyright (C) 2015, Matthias Lieber <matthias.lieber@tu-dresden.de>
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

static inline uint32_t get_cycles() {
    uint32_t val;
    asm volatile (
          "rsr    %0, CCOUNT;"
          : "=a" (val)
    );
    return val;
}

int main() {
    volatile cycles_t *addr = reinterpret_cast<volatile cycles_t*>(CM_CCOUNT_AT_CM);
    cycles_t major = 0;
    uint32_t last = 0;
    while(1) {
        uint32_t now = get_cycles();
        if(now < last)
            major += static_cast<cycles_t>(1) << 32;
        *addr = major | static_cast<cycles_t>(now);
        last = now;
    }
    return 0;
}
