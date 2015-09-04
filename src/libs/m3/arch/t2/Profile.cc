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

#include <m3/util/Sync.h>
#include <m3/util/Profile.h>

namespace m3 {

cycles_t Profile::start(UNUSED unsigned id) {
    return stop(id);
}

cycles_t Profile::stop(UNUSED unsigned id) {
    uint64_t cycles = 0;

    DTU::get().set_target(SLOT_NO, CCOUNT_CORE, CCOUNT_ADDR);
    Sync::memory_barrier();
    DTU::get().fire(SLOT_NO, DTU::READ, &cycles, sizeof(cycles));

    // the number of cycles will never be zero. so wait until it changes
    while(*(volatile uint64_t*)&cycles == 0)
        ;
    return cycles;
}

}
