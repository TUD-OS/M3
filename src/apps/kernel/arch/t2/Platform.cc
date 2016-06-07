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

#include "DTU.h"
#include "Platform.h"

namespace kernel {

static bool initialized = false;
static m3::KernelEnv kernenv;

const m3::KernelEnv &Platform::kenv() {
    if(!initialized) {
        // no modules
        kernenv.mods[0] = 0;

        // init PEs
        kernenv.pes[CM_CORE] = m3::PE(m3::PEType::COMP_IMEM, 128 * 1024);
        for(int i = 0; i < 8; ++i)
            kernenv.pes[FIRST_PE_ID + i] = m3::PE(m3::PEType::COMP_IMEM, 64 * 1024);
        initialized = true;
    }
    return kernenv;
}

const m3::PE &Platform::pe(size_t no) {
    return kenv().pes[no];
}

uintptr_t Platform::def_recvbuf(size_t) {
    return DEF_RCVBUF;
}

uintptr_t Platform::rw_barrier(size_t) {
    // no rw barrier here
    return 1;
}

}
