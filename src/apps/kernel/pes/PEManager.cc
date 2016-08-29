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
    :  _switches(), _ctxswitcher(new ContextSwitcher*[Platform::pe_count()]) {
    for(size_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(Platform::pe(i).is_programmable())
            _ctxswitcher[i] = new ContextSwitcher(i);
        else
            _ctxswitcher[i] = nullptr;
    }
    deprivilege_pes();
}

void PEManager::init() {
    for(size_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(_ctxswitcher[i]) {
            _ctxswitcher[i]->init();
            start_switch(i);
        }
    }
}

void PEManager::add_vpe(VPE *vpe) {
    ContextSwitcher *ctx = _ctxswitcher[vpe->core()];
    assert(ctx);
    if(ctx->enqueue(vpe))
        start_switch(vpe->core());
}

void PEManager::remove_vpe(VPE *vpe) {
    ContextSwitcher *ctx = _ctxswitcher[vpe->core()];
    assert(ctx);
    if(ctx->remove(vpe))
        _switches.append(ctx);
}

void PEManager::start_vpe(VPE *vpe) {
    ContextSwitcher *ctx = _ctxswitcher[vpe->core()];
    assert(ctx);
    if(ctx->start_vpe())
        _switches.append(ctx);
}

void PEManager::start_switch(int pe) {
    ContextSwitcher *ctx = _ctxswitcher[pe];
    assert(ctx);
    if(ctx->start_switch())
        _switches.append(ctx);
}

bool PEManager::continue_switches() {
    for(auto it = _switches.begin(); it != _switches.end(); ) {
        auto old = it++;
        if(!old->continue_switch())
            _switches.remove(&*old);
    }
    return _switches.length() > 0;
}

int PEManager::find_pe(const m3::PEDesc &pe, bool tmuxable) {
    size_t i;
    for(i = Platform::first_pe(); i <= Platform::last_pe(); ++i) {
        if(!_ctxswitcher[i])
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
    for(size_t i = Platform::first_pe(); i <= Platform::last_pe(); ++i)
        DTU::get().deprivilege(i);
}

}
