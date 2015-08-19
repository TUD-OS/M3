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

#include <m3/cap/Gate.h>
#include <m3/cap/RecvGate.h>
#include <m3/cap/VPE.h>
#include <m3/ChanMng.h>
#include <m3/Syscalls.h>
#include <m3/Errors.h>
#include <m3/Log.h>
#include <c/div.h>

namespace m3 {

ChanMng ChanMngBase::_inst INIT_PRIORITY(103);

ChanMngBase::ChanMngBase()
    : _next_victim(1), _chanswitcher(new ChanSwitcher), _gates() {
}

void ChanSwitcher::switch_chan(size_t victim, capsel_t oldcap, capsel_t newcap) {
    if(Syscalls::get().activate(victim, oldcap, newcap) != Errors::NO_ERROR) {
        // if we wanted to deactivate a cap, we can ignore the failure
        if(newcap != Cap::INVALID)
            PANIC("Unable to arm SEP " << victim << ": " << Errors::last);
    }
}

void ChanMngBase::reserve(size_t i) {
    // take care that some non-fixed gate could already use that channel
    if(_gates[i]) {
        _chanswitcher->switch_chan(i, _gates[i]->sel(), Cap::INVALID);
        _gates[i]->_chanid = Gate::UNBOUND;
        if(_gates[i]->type() == Cap::RECV_GATE) {
            RecvGate *rgate = static_cast<RecvGate*>(_gates[i]);
            rgate->buffer()->attach(Gate::UNBOUND);
        }
        _gates[i] = nullptr;
    }
    static_cast<ChanMng*>(this)->set_msgcnt(i, 0);
}

void ChanMngBase::switch_to(Gate *gate) {
    size_t victim = select_victim();
    _chanswitcher->switch_chan(victim, _gates[victim] ? _gates[victim]->sel() : Cap::INVALID, gate->sel());
    _gates[victim] = gate;
    gate->_chanid = victim;
}

void ChanMngBase::switch_cap(Gate *gate, capsel_t newcap) {
    if(gate->chanid() != Gate::UNBOUND) {
        _chanswitcher->switch_chan(gate->chanid(), gate->sel(), newcap);
        if(newcap == Cap::INVALID) {
            _gates[gate->chanid()] = nullptr;
            gate->_chanid = Gate::UNBOUND;
        }
    }
}

void ChanMngBase::remove(Gate *gate, bool unarm) {
    if(gate->_chanid != Gate::NODESTROY && gate->_chanid != Gate::UNBOUND && gate->sel() != Cap::INVALID) {
        assert(_gates[gate->_chanid] == nullptr || _gates[gate->_chanid] == gate);
        if(unarm) {
            // we have to "disarm" our channel, i.e. set the registers to zero. otherwise the cmpxchg
            // will fail when we program the next gate on this channel.
            // note that the kernel has to validate that it is 0 for "unused channels" because otherwise
            // we could just specify that our channel is unused and the kernel won't check it and thereby
            // trick the whole system.
            _chanswitcher->switch_chan(gate->_chanid, gate->sel(), Cap::INVALID);
        }
        _gates[gate->_chanid] = nullptr;
        gate->_chanid = Gate::UNBOUND;
    }
}

size_t ChanMngBase::select_victim() {
    size_t count = 0;
    size_t victim = _next_victim;
    while(!VPE::self().is_chan_free(victim) && count++ < CHAN_COUNT) {
        // victim = (victim + 1) % CHAN_COUNT
        long rem;
        divide(victim + 1, CHAN_COUNT, &rem);
        victim = rem;
    }
    if(!VPE::self().is_chan_free(victim))
        PANIC("No free channels for multiplexing");
    if(_gates[victim] != nullptr)
        _gates[victim]->_chanid = Gate::UNBOUND;

    // _next_victim = (victim + 1) % CHAN_COUNT
    long rem;
    divide(victim + 1, CHAN_COUNT, &rem);
    _next_victim = rem;
    return victim;
}

}
