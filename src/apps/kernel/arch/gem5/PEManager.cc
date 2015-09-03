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

#include "../../PEManager.h"

extern int tempchan;

namespace m3 {

void PEManager::deprivilege_pes() {
    for(int i = 0; i < AVAIL_PES; ++i) {
        // unset the privileged flag (writes to other bits are ignored)
        DTU::reg_t status = 0;
        Sync::compiler_barrier();
        static_assert(offsetof(DTU::DtuRegs, status) == 0, "Status register is not at offset 0");
        DTU::get().configure_mem(tempchan, APP_CORES + i, (uintptr_t)DTU::dtu_regs(), sizeof(status));
        DTU::get().write(tempchan, &status, sizeof(status), 0);
    }
}

PEManager::~PEManager() {
    for(size_t i = 0; i < AVAIL_PES; ++i) {
        if(_vpes[i])
            _vpes[i]->unref();
    }
}

}
