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

#include <m3/DTU.h>
#include <m3/Log.h>

#include "../../Capability.h"
#include "../../PEManager.h"

namespace m3 {

Errors::Code MsgCapability::revoke() {
    if(localepid != -1) {
        // overwrite the SEP regs with 0's to set the credits to 0, which effectively disables the
        // endpoint.
        KVPE &vpe = PEManager::get().vpe(table()->id() - 1);
        word_t regs[DTU::EPS_RCNT];
        memset(regs, 0, sizeof(regs));
        size_t offset = localepid * DTU::EPS_RCNT * sizeof(word_t);
        vpe.seps_gate().write_sync(regs, sizeof(regs), offset);
    }
    obj.unref();
    return Errors::NO_ERROR;
}

}
