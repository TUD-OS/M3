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
    uintptr_t addr = m3::DTU::noc_to_virt(reinterpret_cast<uintptr_t>(m3::env()->kenv));
    DTU::get().read_mem_at(MEMORY_CORE, 0, addr, this, sizeof(*this));
}

uintptr_t Platform::def_recvbuf(size_t no) {
    return rw_barrier(no);
}

uintptr_t Platform::rw_barrier(size_t no) {
    if(pe(no).has_virtmem())
        return RECVBUF_SPACE;
    return pe(no).mem_size() - RECVBUF_SIZE_SPM;
}

}
