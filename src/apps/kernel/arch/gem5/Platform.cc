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
#include "mem/MemoryModule.h"
#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

INIT_PRIO_USER(2) Platform::KEnv Platform::_kenv;

// note that we currently assume here, that memory PEs are first, followed by all compute PEs
static size_t last_pe_id;

Platform::KEnv::KEnv() {
    // the KernelEnv is stored in the first PE (memory PE)
    uintptr_t addr = m3::DTU::noc_to_virt(reinterpret_cast<uintptr_t>(m3::env()->kenv));
    DTU::get().read_mem(VPEDesc(0, 0), addr, this, sizeof(*this));

    // register memory modules
    int count = 0;
    const size_t USABLE_MEM  = 256 * 1024 * 1024;
    MainMemory &mem = MainMemory::get();
    for(size_t i = 0; i < pe_count; ++i) {
        if(pes[i].type() == m3::PEType::MEM) {
            // the first memory module hosts the FS image and other stuff
            if(count == 0) {
                mem.add(new MemoryModule(false, i, 0, USABLE_MEM));
                mem.add(new MemoryModule(true, i, USABLE_MEM, pes[i].mem_size() - USABLE_MEM));
            }
            else
                mem.add(new MemoryModule(true, i, 0, pes[i].mem_size()));
            count++;
        }
        else
            last_pe_id = i;
    }
}

size_t Platform::kernel_pe() {
    // gem5 initializes the coreid for us
    return m3::env()->coreid;
}
size_t Platform::first_pe() {
    return m3::env()->coreid + 1;
}
size_t Platform::last_pe() {
    return last_pe_id;
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
