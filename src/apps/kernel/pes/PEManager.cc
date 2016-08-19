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

#include <base/stream/OStringStream.h>
#include <base/log/Kernel.h>
#include <base/Panic.h>

#include <string.h>

#include "pes/PEManager.h"
#include "Platform.h"

namespace kernel {

PEManager *PEManager::_inst;

PEManager::PEManager()
#if defined(__t3__) || defined(__gem5__)
    :  _ctxswitcher(new ContextSwitcher*[Platform::pe_count()])
#endif
{
    for(size_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i)
        _ctxswitcher[i] = new ContextSwitcher(i);
    deprivilege_pes();
}

int PEManager::find_pe(const m3::PEDesc &pe, bool tmuxable) {
    size_t i;
    for(i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if((_ctxswitcher[i]->count() == 0 || tmuxable) &&
            Platform::pe(i).isa() == pe.isa() && Platform::pe(i).type() == pe.type())
            break;
    }
    if(i > Platform::last_pe())
        return 0;
    return i;
}

void PEManager::deprivilege_pes() {
    for(size_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i)
        DTU::get().deprivilege(i);
}

}
