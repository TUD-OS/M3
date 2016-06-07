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

#include "DTU.h"
#include "Platform.h"

namespace kernel {

INIT_PRIO_USER(2) Platform::KEnv Platform::_kenv;

Platform::KEnv::KEnv() {
    // no modules
    mods[0] = 0;

    // init PEs
    pe_count = MAX_CORES;
    pes[CM_CORE] = m3::PE(m3::PEType::COMP_IMEM, 128 * 1024).value();
    for(int i = 0; i < 8; ++i)
        pes[FIRST_PE_ID + i] = m3::PE(m3::PEType::COMP_IMEM, 64 * 1024).value();
}

uintptr_t Platform::def_recvbuf(size_t) {
    return DEF_RCVBUF;
}

uintptr_t Platform::rw_barrier(size_t) {
    // no rw barrier here
    return 1;
}

}
