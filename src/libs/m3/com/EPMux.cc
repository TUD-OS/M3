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

#include <base/Errors.h>
#include <base/Log.h>

#include <m3/com/EPMux.h>
#include <m3/com/Gate.h>
#include <m3/com/RecvGate.h>
#include <m3/VPE.h>
#include <m3/Syscalls.h>

#include <c/div.h>

namespace m3 {

EPMux EPMux::_inst INIT_PRIORITY(103);

EPMux::EPMux()
    : _next_victim(1), _gates() {
}

void EPMux::reserve(size_t ep) {
    // take care that some non-fixed gate could already use that endpoint
    if(_gates[ep]) {
        switch_ep(ep, _gates[ep]->sel(), ObjCap::INVALID);
        _gates[ep]->_epid = Gate::UNBOUND;
        if(_gates[ep]->type() == ObjCap::RECV_GATE) {
            RecvGate *rgate = static_cast<RecvGate*>(_gates[ep]);
            rgate->buffer()->attach(Gate::UNBOUND);
        }
        _gates[ep] = nullptr;
    }
}

void EPMux::switch_to(Gate *gate) {
    size_t victim = select_victim();
    switch_ep(victim, _gates[victim] ? _gates[victim]->sel() : ObjCap::INVALID, gate->sel());
    _gates[victim] = gate;
    gate->_epid = victim;
}

void EPMux::switch_cap(Gate *gate, capsel_t newcap) {
    if(gate->epid() != Gate::UNBOUND) {
        switch_ep(gate->epid(), gate->sel(), newcap);
        if(newcap == ObjCap::INVALID) {
            _gates[gate->epid()] = nullptr;
            gate->_epid = Gate::UNBOUND;
        }
    }
}

void EPMux::remove(Gate *gate, bool invalidate) {
    if(gate->_epid != Gate::NODESTROY && gate->_epid != Gate::UNBOUND && gate->sel() != ObjCap::INVALID) {
        assert(_gates[gate->_epid] == nullptr || _gates[gate->_epid] == gate);
        if(invalidate) {
            // we have to invalidate our endpoint, i.e. set the registers to zero. otherwise the cmpxchg
            // will fail when we program the next gate on this endpoint.
            // note that the kernel has to validate that it is 0 for "unused endpoints" because otherwise
            // we could just specify that our endpoint is unused and the kernel won't check it and thereby
            // trick the whole system.
            switch_ep(gate->_epid, gate->sel(), ObjCap::INVALID);
        }
        _gates[gate->_epid] = nullptr;
        gate->_epid = Gate::UNBOUND;
    }
}

void EPMux::reset() {
    for(int i = 0; i < EP_COUNT; ++i) {
        if(_gates[i])
            _gates[i]->_epid = Gate::UNBOUND;
        _gates[i] = nullptr;
    }
}

size_t EPMux::select_victim() {
    size_t count = 0;
    size_t victim = _next_victim;
    while(!VPE::self().is_ep_free(victim) && count++ < EP_COUNT) {
        // victim = (victim + 1) % EP_COUNT
        long rem;
        divide(victim + 1, EP_COUNT, &rem);
        victim = rem;
    }
    if(!VPE::self().is_ep_free(victim))
        PANIC("No free endpoints for multiplexing");
    if(_gates[victim] != nullptr)
        _gates[victim]->_epid = Gate::UNBOUND;

    // _next_victim = (victim + 1) % EP_COUNT
    long rem;
    divide(victim + 1, EP_COUNT, &rem);
    _next_victim = rem;
    return victim;
}

void EPMux::switch_ep(size_t victim, capsel_t oldcap, capsel_t newcap) {
    if(Syscalls::get().activate(victim, oldcap, newcap) != Errors::NO_ERROR) {
        // if we wanted to deactivate a cap, we can ignore the failure
        if(newcap != ObjCap::INVALID)
            PANIC("Unable to arm SEP " << victim << ": " << Errors::last);
    }
}

}
