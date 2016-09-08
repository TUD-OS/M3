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
    :  _ctxswitcher(new ContextSwitcher*[Platform::pe_count()]) {
    for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(Platform::pe(i).is_programmable())
            _ctxswitcher[i] = new ContextSwitcher(i);
        else
            _ctxswitcher[i] = nullptr;
    }
    deprivilege_pes();
}

void PEManager::init() {
    for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(_ctxswitcher[i])
            _ctxswitcher[i]->init();
    }
}

void PEManager::add_vpe(VPE *vpe) {
    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    assert(ctx);
    ctx->add_vpe(vpe);
}

void PEManager::remove_vpe(VPE *vpe) {
    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    assert(ctx);
    ctx->remove_vpe(vpe);
}

void PEManager::start_vpe(VPE *vpe) {
    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    assert(ctx);
    ctx->start_vpe(vpe);
}

void PEManager::stop_vpe(VPE *vpe) {
    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    assert(ctx);
    ctx->stop_vpe(vpe);
}

void PEManager::migrate_vpe(VPE *vpe) {
    peid_t npe = find_pe(Platform::pe(vpe->pe()), vpe->pe(), true);
    if(npe == 0)
        return;

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    assert(ctx);
    ctx->remove_vpe(vpe);

    vpe->set_pe(npe);

    ctx = _ctxswitcher[npe];
    assert(ctx);
    ctx->add_vpe(vpe);
}

void PEManager::yield_vpe(VPE *vpe) {
    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    assert(ctx);
    ctx->yield_vpe(vpe);
}

void PEManager::unblock_vpe(VPE *vpe) {
    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    assert(ctx);
    ctx->unblock_vpe(vpe);
}

peid_t PEManager::find_pe(const m3::PEDesc &pe, peid_t except, bool tmuxable) {
    size_t i;
    for(i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(!_ctxswitcher[i])
            continue;

        if(i == except)
            continue;

        if(Platform::pe(i).isa() != pe.isa() || Platform::pe(i).type() != pe.type())
            continue;

        if(_ctxswitcher[i]->count() == 0)
            break;

        // TODO temporary
        if(tmuxable && _ctxswitcher[i]->can_mux())
            break;
    }
    if(i > Platform::last_pe())
        return 0;
    return i;
}

void PEManager::deprivilege_pes() {
    for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i)
        DTU::get().deprivilege(i);
}

}
