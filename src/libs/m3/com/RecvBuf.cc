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

#include <base/log/Lib.h>
#include <base/Init.h>
#include <base/Panic.h>

#include <m3/com/RecvGate.h>
#include <m3/com/EPMux.h>
#include <m3/com/RecvBuf.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

namespace m3 {

INIT_PRIO_RECVBUF RecvBuf RecvBuf::_default (
#if defined(__host__) || defined(__gem5__)
    RecvBuf::create(DTU::DEF_RECVEP, nextlog2<256>::val, nextlog2<128>::val, 0)
#else
    RecvBuf::bindto(DTU::DEF_RECVEP, reinterpret_cast<void*>(DEF_RCVBUF), DEF_RCVBUF_ORDER, 0)
#endif
);

void RecvBuf::RecvBufWorkItem::work() {
    DTU &dtu = DTU::get();
    assert(_epid != UNBOUND);
    DTU::Message *msg = dtu.fetch_msg(_epid);
    if(msg) {
        LLOG(IPC, "Received msg @ " << (void*)msg << " over ep " << _epid);
        RecvGate *gate = reinterpret_cast<RecvGate*>(msg->label);
        GateIStream is(*gate, msg);
        gate->notify_all(is);
    }
}

void RecvBuf::attach(size_t i) {
    _epid = i;
    if(i != UNBOUND) {
        // first reserve the endpoint; we might need to invalidate it
        EPMux::get().reserve(i);

#if defined(__t3__)
        // required for t3 because one can't write to these registers externally
        DTU::get().configure_recv(epid(), reinterpret_cast<word_t>(addr()), order(),
            msgorder(), flags());
#endif

        if(epid() > DTU::DEF_RECVEP) {
            Errors::Code res = Syscalls::get().attachrb(VPE::self().sel(), epid(),
                reinterpret_cast<word_t>(addr()), order(), msgorder(), flags());
            if(res != Errors::NO_ERROR)
                PANIC("Attaching recvbuf to " << epid() << " failed: " << Errors::to_string(res));
        }

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
    if(epid() > DTU::DEF_RECVEP && epid() != RecvBuf::UNBOUND)
        Syscalls::get().detachrb(VPE::self().sel(), epid());
    _epid = UNBOUND;

    disable();
}

}
