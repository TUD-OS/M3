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

#include <base/util/Time.h>
#include <base/CPU.h>

namespace m3 {

cycles_t Time::start(UNUSED unsigned id) {
    return stop(id);
}

cycles_t Time::stop(UNUSED unsigned id) {
    cycles_t cycles = 0;

    DTU::get().set_target(SLOT_NO, CCOUNT_CORE, CCOUNT_ADDR);
    CPU::memory_barrier();
    DTU::get().fire(SLOT_NO, DTU::READ, &cycles, sizeof(cycles));

    // the number of cycles will never be zero. so wait until it changes
    while(*(volatile cycles_t*)&cycles == 0)
        ;
    return cycles;
}

}
