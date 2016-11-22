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
#include <base/tracing/Tracing.h>

#include "pes/Timeouts.h"
#include "DTU.h"

namespace kernel {

Timeouts Timeouts::_inst;

cycles_t Timeouts::sleep_time() const {
    // sleep until waked up by a message if there is no pending timeout
    if(_timeouts.length() == 0)
        return 0;

    cycles_t now = DTU::get().get_time();
    // do not sleep if there are timeouts to trigger
    if(_timeouts.begin()->when <= now)
        return static_cast<cycles_t>(-1);

    // sleep until the next timeout or until we receive a message
    return _timeouts.begin()->when - now;
}

void Timeouts::trigger() {
    // exit early if nothing to do
    if(_timeouts.length() == 0)
        return;

    EVENT_TRACER_Kernel_Timeouts();
    cycles_t now = DTU::get().get_time();
    do {
        auto &to = *_timeouts.begin();
        // timeouts are sorted, so stop if we hit a future timeout
        if(to.when > now)
            break;

        KLOG(TIMEOUTS, "Triggering timeout " << &to << " (now=" << now << ", due=" << to.when << ")");
        // remove it first to get into a consistent state; the callback might do a thread switch
        _timeouts.remove_first();
        to.callback();
        delete &to;
    }
    while(_timeouts.length() > 0);
}

Timeout *Timeouts::wait_for(cycles_t cycles, std::function<void()> callback) {
    cycles_t when = DTU::get().get_time() + cycles;

    // insert timeouts in ascending order
    Timeout *prev = nullptr;
    for(auto it = _timeouts.begin(); it != _timeouts.end(); ++it) {
        if(it->when >= when)
            break;
        prev = &*it;
    }

    Timeout *to = new Timeout(when, callback);
    KLOG(TIMEOUTS, "Inserting timeout " << to << " (due=" << to->when << ")");
    _timeouts.insert(prev, to);
    return to;
}

void Timeouts::cancel(Timeout *to) {
    KLOG(TIMEOUTS, "Canceling timeout " << to << " (due=" << to->when << ")");
    _timeouts.remove(to);
    delete to;
}

}
