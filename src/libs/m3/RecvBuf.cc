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

#include <m3/cap/VPE.h>
#include <m3/RecvBuf.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>

namespace m3 {

void RecvBuf::RecvBufWorkItem::work() {
    DTU &dtu = DTU::get();
    assert(_epid != UNBOUND && dtu.uses_header(_epid));
    if(dtu.fetch_msg(_epid)) {
        DTU::Message *msg = dtu.message(_epid);
        LOG(IPC, "Received msg @ " << (void*)msg << " over ep " << _epid);
        RecvGate *gate = reinterpret_cast<RecvGate*>(msg->label);
        gate->notify_all();
        dtu.ack_message(_epid);
    }
}

void RecvBuf::attach(size_t i) {
    if(i != UNBOUND) {
        // first reserve the endpoint; we might need to invalidate it
        EPMux::get().reserve(i);

#if !defined(__t3__)
        // always required for t3 because one can't write to these registers externally
        if(coreid() == KERNEL_CORE) {
#endif
            DTU::get().configure_recv(i, reinterpret_cast<word_t>(_buf), _order, _msgorder,
                _flags & ~DELETE_BUF);
#if !defined(__t3__)
        }
#endif

        if(coreid() != KERNEL_CORE && i > DTU::DEF_RECVEP) {
            if(Syscalls::get().attachrb(VPE::self().sel(), i, reinterpret_cast<word_t>(_buf),
                    _order, _msgorder, _flags & ~DELETE_BUF) != Errors::NO_ERROR)
                PANIC("Attaching receive buffer to " << i << " failed: " << Errors::to_string(Errors::last));
        }

        // if we may receive messages from the endpoint, create a worker for it
        // TODO hack for host: we don't want to do that for MEM_EP but know that only afterwards
        // because the EPs are not initialized yet
        if(i != DTU::MEM_EP && DTU::get().uses_header(i)) {
            if(_workitem == nullptr) {
                _workitem = new RecvBufWorkItem(i);
                WorkLoop::get().add(_workitem, i == DTU::MEM_EP || i == DTU::DEF_RECVEP);
            }
            else
                _workitem->epid(i);
        }
    }
    // if it's UNBOUND now, don't use the worker again, if there is any
    else if(_workitem)
        detach();

    _epid = i;
}

void RecvBuf::disable() {
    if(_workitem) {
        WorkLoop::get().remove(_workitem);
        delete _workitem;
        _workitem = nullptr;
    }
}

void RecvBuf::detach() {
    if(coreid() != KERNEL_CORE && _epid > DTU::DEF_RECVEP && _epid != UNBOUND) {
        Syscalls::get().detachrb(VPE::self().sel(), _epid);
        _epid = UNBOUND;
    }

    disable();
}

}
