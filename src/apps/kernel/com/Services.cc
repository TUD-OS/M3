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
#include "SyscallHandler.h"

namespace kernel {

ServiceList ServiceList::_inst;

Service::Service(VPE &vpe, capsel_t sel, const m3::String &name, epid_t ep, label_t label, int capacity)
    : m3::SListItem(), RefCounted(), closing(), _vpe(vpe), _sel(sel), _name(name),
      _rgate(SyscallHandler::get().srvepid(), nullptr), _sgate(vpe, ep, label), _queue(capacity) {
}

Service::~Service() {
    // we have allocated the selector and stored it in our cap-table on creation; undo that
    ServiceList::get().remove(this);
}

void ServiceList::send_and_receive(m3::Reference<Service> serv, const void *msg, size_t size, bool free) {
    serv->recv_gate().subscribe([this, serv] (GateIStream &, m3::Subscriber<GateIStream&> *s) {
        m3::Reference<Service> srvcpy = serv;
        srvcpy->received_reply();
        srvcpy->recv_gate().unsubscribe(s);
    });

    serv->send(&serv->recv_gate(), msg, size, free);
}

}
