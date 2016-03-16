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

#include <m3/cap/RecvGate.h>
#include <m3/EPMux.h>
#include <m3/RecvBuf.h>
#include <m3/Log.h>

namespace m3 {

void RecvBuf::RecvBufWorkItem::work() {
    DTU &dtu = DTU::get();
    assert(_epid != UNBOUND);
    if(dtu.fetch_msg(_epid)) {
        DTU::Message *msg = dtu.message(_epid);
        LOG(IPC, "Received msg @ " << (void*)msg << " over ep " << _epid);
        RecvGate *gate = reinterpret_cast<RecvGate*>(msg->label);
        gate->notify_all();
        dtu.ack_message(_epid);
    }
}

void RecvBuf::attach(size_t i) {
    _epid = i;
    if(i != UNBOUND) {
        // first reserve the endpoint; we might need to invalidate it
        EPMux::get().reserve(i);

        env()->backend->attach_recvbuf(this);

        // if we may receive messages from the endpoint, create a worker for it
        // TODO hack for host: we don't want to do that for MEM_EP but know that only afterwards
        // because the EPs are not initialized yet
        if(i != DTU::MEM_EP && (~_flags & DTU::FLAG_NO_HEADER)) {
            if(_workitem == nullptr) {
                _workitem = new RecvBufWorkItem(i);
                env()->workloop()->add(_workitem, i == DTU::MEM_EP || i == DTU::DEF_RECVEP);
            }
            else
                _workitem->epid(i);
        }
    }
    // if it's UNBOUND now, don't use the worker again, if there is any
    else if(_workitem)
        detach();
}

void RecvBuf::disable() {
    if(_workitem) {
        env()->workloop()->remove(_workitem);
        delete _workitem;
        _workitem = nullptr;
    }
}

void RecvBuf::detach() {
    env()->backend->detach_recvbuf(this);
    _epid = UNBOUND;

    disable();
}

}
