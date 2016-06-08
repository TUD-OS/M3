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

#include <base/Init.h>

#include "mem/MainMemory.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

INIT_PRIO_USER(2) Platform::KEnv Platform::_kenv;

Platform::KEnv::KEnv() {
    // no modules
    mods[0] = 0;

    // init PEs
    pe_count = MAX_CORES + 1;
    for(int i = 0; i < MAX_CORES; ++i)
        pes[i] = m3::PE(m3::PEType::COMP_IMEM, 64 * 1024);
    pes[MAX_CORES] = m3::PE(m3::PEType::MEM, 512 * 1024 * 1024);

    // register memory modules
    MainMemory &mem = MainMemory::get();
    const size_t USABLE_MEM  = 64 * 1024 * 1024;
    mem.add(new MemoryModule(false, MAX_CORES, 0, USABLE_MEM));
    mem.add(new MemoryModule(true, MAX_CORES, USABLE_MEM, pes[MAX_CORES].mem_size() - USABLE_MEM));
}

size_t Platform::kernel_pe() {
    return 0;
}
size_t Platform::first_pe() {
    return 1;
}
size_t Platform::last_pe() {
    return _kenv.pe_count - 2;
}

uintptr_t Platform::def_recvbuf(size_t) {
    return DEF_RCVBUF;
}

uintptr_t Platform::rw_barrier(size_t) {
    // no rw barrier here
    return 1;
}

}
