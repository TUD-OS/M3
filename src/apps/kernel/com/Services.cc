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

#include <base/Common.h>

#include "com/Services.h"
#include "pes/VPE.h"

namespace kernel {

ServiceList ServiceList::_inst;

Service::Service(VPE &vpe, capsel_t sel, const m3::String &name, const m3::Reference<RGateObject> &rgate)
    : m3::SListItem(),
      RefCounted(),
      _squeue(vpe),
      _sel(sel),
      _name(name),
      _sgate(vpe, rgate->ep, 0),
      _rgate(rgate) {
}

Service::~Service() {
    // we have allocated the selector and stored it in our cap-table on creation; undo that
    ServiceList::get().remove(this);
}

int Service::pending() const {
    return _squeue.inflight() + _squeue.pending();
}

void Service::send(const void *msg, size_t size, bool free) {
    if(!_rgate->activated())
        return;

    _squeue.send(&_sgate, msg, size, free);
}

const m3::DTU::Message *Service::send_receive(const void *msg, size_t size, bool free) {
    if(!_rgate->activated())
        return nullptr;

    event_t event = _squeue.send(&_sgate, msg, size, free);

    m3::ThreadManager::get().wait_for(event);

    return reinterpret_cast<const m3::DTU::Message*>(m3::ThreadManager::get().get_current_msg());
}

}
