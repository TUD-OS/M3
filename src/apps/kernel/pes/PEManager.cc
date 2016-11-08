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

#include <base/log/Kernel.h>

#include "pes/PEManager.h"
#include "pes/VPEManager.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

PEManager *PEManager::_inst;

PEManager::PEManager()
    : _ctxswitcher(new ContextSwitcher*[Platform::pe_count()]) {
    for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(Platform::pe(i).supports_ctxsw())
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

void PEManager::update_yield(size_t before) {
    if((before == 0 && ContextSwitcher::global_ready() > 0) ||
       (before > 0 && ContextSwitcher::global_ready() == 0)) {
        for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
            if(_ctxswitcher[i])
                _ctxswitcher[i]->update_yield();
        }
    }
}

void PEManager::add_vpe(VPE *vpe) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    if(ctx)
        ctx->add_vpe(vpe);

    update_yield(global);
}

void PEManager::remove_vpe(VPE *vpe) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    if(ctx)
        ctx->remove_vpe(vpe);

    update_yield(global);
}

void PEManager::start_vpe(VPE *vpe) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    if(ctx)
        ctx->start_vpe(vpe);

    update_yield(global);
}

void PEManager::stop_vpe(VPE *vpe) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    if(ctx)
        ctx->stop_vpe(vpe);

    update_yield(global);
}

bool PEManager::migrate_vpe(VPE *vpe) {
    peid_t npe = find_pe(Platform::pe(vpe->pe()), vpe->pe(), true);
    if(npe == 0)
        return false;

    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    if(!ctx)
        return false;
    ctx->remove_vpe(vpe);

    vpe->set_pe(npe);

    ctx = _ctxswitcher[npe];
    assert(ctx);
    ctx->add_vpe(vpe);

    update_yield(global);
    return true;
}

void PEManager::yield_vpe(VPE *vpe) {
    size_t global = ContextSwitcher::global_ready();

    peid_t pe = vpe->pe();
    ContextSwitcher *ctx = _ctxswitcher[pe];
    assert(ctx);
    // check if there is somebody else on the current PE
    if(!ctx->yield_vpe(vpe)) {
        m3::PEDesc pedesc = Platform::pe(pe);

        // try to steal a VPE from a different PE
        for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
            if(!_ctxswitcher[i] ||
                i == pe ||
                Platform::pe(i).isa() != pedesc.isa() ||
                Platform::pe(i).type() != pedesc.type())
                continue;

            VPE *nvpe = _ctxswitcher[i]->steal_vpe();
            if(!nvpe)
                continue;

            KLOG(VPES, "Stole VPE " << nvpe->id() << " from " << i << " to " << pe);

            nvpe->set_pe(pe);
            _ctxswitcher[pe]->add_vpe(nvpe);

            _ctxswitcher[pe]->unblock_vpe(nvpe, true);
            break;
        }
    }

    update_yield(global);
}

bool PEManager::unblock_vpe(VPE *vpe, bool force) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    assert(ctx);
    bool res = ctx->unblock_vpe(vpe, force);

    update_yield(global);
    return res;
}

peid_t PEManager::find_pe(const m3::PEDesc &pe, peid_t except, bool tmuxable) {
    peid_t choice = 0;
    uint others = VPEManager::MAX_VPES;
    for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(i == except ||
           Platform::pe(i).isa() != pe.isa() ||
           Platform::pe(i).type() != pe.type())
            continue;

        if(!_ctxswitcher[i] || _ctxswitcher[i]->count() == 0)
            return i;

        // TODO temporary
        if(tmuxable && _ctxswitcher[i]->can_mux() && _ctxswitcher[i]->count() < others) {
            choice = i;
            others = _ctxswitcher[i]->count();
        }
    }
    return choice;
}

void PEManager::deprivilege_pes() {
    for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i)
        DTU::get().deprivilege(i);
}

}
