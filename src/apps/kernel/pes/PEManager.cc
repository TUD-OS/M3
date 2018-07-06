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
#include "pes/VPEGroup.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

PEManager *PEManager::_inst;

PEManager::PEManager()
    : _ctxswitcher(new ContextSwitcher*[Platform::pe_count()]),
      _used(new bool[Platform::pe_count()]) {
    for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(Platform::pe(i).supports_vpes())
            _ctxswitcher[i] = new ContextSwitcher(i);
        else
            _ctxswitcher[i] = nullptr;
        _used[i] = false;
    }
    deprivilege_pes();
}

void PEManager::init() {
    for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(_ctxswitcher[i])
            _ctxswitcher[i]->init();
    }
}

VPE *PEManager::current(peid_t pe) const {
    ContextSwitcher *ctx = _ctxswitcher[pe];
    if(ctx)
        return ctx->current();
    return nullptr;
}

bool PEManager::yield(peid_t pe) {
    ContextSwitcher *ctx = _ctxswitcher[pe];
    if(ctx) {
        VPE *cur = ctx->current();
        if(cur)
            return ctx->yield_vpe(cur);
    }
    return false;
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
    else
        _used[vpe->pe()] = true;

    update_yield(global);
}

void PEManager::remove_vpe(VPE *vpe) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    if(ctx) {
        ctx->remove_vpe(vpe);
        // if there is no one left, try to steal a VPE from somewhere else
        if(ctx->ready() == 0)
            steal_vpe(vpe->pe());
    }
    else
        _used[vpe->pe()] = false;

    update_yield(global);
}

void PEManager::start_vpe(VPE *vpe) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    if(ctx)
        ctx->start_vpe(vpe);
    else {
        vpe->_dtustate.restore(VPEDesc(vpe->pe(), VPE::INVALID_ID), 0, vpe->id());
        vpe->_state = VPE::RUNNING;
        vpe->init_memory();
    }

    update_yield(global);
}

void PEManager::stop_vpe(VPE *vpe) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    if(ctx) {
        ctx->stop_vpe(vpe);
        if(ctx->ready() == 0)
            steal_vpe(vpe->pe());
    }
    else {
        DTU::get().unset_vpeid(vpe->desc());
        vpe->_state = VPE::SUSPENDED;
    }

    update_yield(global);
}

bool PEManager::migrate_vpe(VPE *vpe, bool fast) {
    peid_t npe = find_pe(Platform::pe(vpe->pe()), vpe->pe(), VPE::F_MUXABLE, nullptr);
    if(npe == 0)
        return false;

    return migrate_to(vpe, npe, fast);
}

bool PEManager::migrate_for(VPE *vpe, VPE *dst) {
    if(vpe->pe() == dst->pe())
        return migrate_vpe(vpe, true);
    if(!_ctxswitcher[dst->pe()]->can_mux())
        return false;

    return migrate_to(vpe, dst->pe(), true);
}

bool PEManager::migrate_to(VPE *vpe, peid_t npe, bool fast) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    // only migrate if we can directly switch to this VPE on the other PE
    if(!ctx || (fast && !_ctxswitcher[npe]->can_switch()))
        return false;
    ctx->remove_vpe(vpe, true);

    vpe->set_pe(npe);
    vpe->needs_invalidate();

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
    if(!ctx->yield_vpe(vpe))
        steal_vpe(pe);

    update_yield(global);
}

void PEManager::steal_vpe(peid_t pe) {
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
        nvpe->needs_invalidate();
        _ctxswitcher[pe]->add_vpe(nvpe);

        _ctxswitcher[pe]->unblock_vpe(nvpe, true);
        break;
    }
}

bool PEManager::unblock_vpe(VPE *vpe, bool force) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    assert(ctx);
    bool res = ctx->unblock_vpe(vpe, force);

    update_yield(global);
    return res;
}

bool PEManager::unblock_vpe_now(VPE *vpe) {
    size_t global = ContextSwitcher::global_ready();

    ContextSwitcher *ctx = _ctxswitcher[vpe->pe()];
    assert(ctx);
    bool res = ctx->unblock_vpe_now(vpe);

    update_yield(global);
    return res;
}

peid_t PEManager::find_pe(const m3::PEDesc &pe, peid_t except, uint flags, const VPEGroup *group) {
    peid_t choice = 0;
    uint others = VPEManager::MAX_VPES;
    for(peid_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(i == except ||
           Platform::pe(i).isa() != pe.isa() ||
           Platform::pe(i).type() != pe.type())
            continue;

        if(!_ctxswitcher[i]) {
            if(_used[i])
                continue;
            return i;
        }

        if(_ctxswitcher[i]->count() == 0)
            return i;

        // TODO temporary
        if((flags & VPE::F_MUXABLE) && _ctxswitcher[i]->can_mux() && _ctxswitcher[i]->count() < others) {
            if(group && group->is_pe_used(i))
                continue;
            if((flags & VPE::F_PINNED) && _ctxswitcher[i]->has_pinned())
                continue;
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
