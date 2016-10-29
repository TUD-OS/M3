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

INIT_PRIO_RECVBUF RecvBuf RecvBuf::_syscall (
#if defined(__host__) || defined(__gem5__)
    VPE::self(), ObjCap::INVALID, DTU::SYSC_REP, nullptr, m3::nextlog2<SYSC_RBUF_SIZE>::val, SYSC_RBUF_ORDER, 0
#else
    VPE::self(), ObjCap::INVALID, DTU::SYSC_REP, reinterpret_cast<void*>(DEF_RCVBUF),
        DEF_RCVBUF_MSGORDER, DEF_RCVBUF_MSGORDER, 0
#endif
);

INIT_PRIO_RECVBUF RecvBuf RecvBuf::_upcall (
    VPE::self(), ObjCap::INVALID, DTU::UPCALL_REP, nullptr, m3::nextlog2<UPCALL_RBUF_SIZE>::val, UPCALL_RBUF_ORDER, 0
);

INIT_PRIO_RECVBUF RecvBuf RecvBuf::_default (
    VPE::self(), ObjCap::INVALID, DTU::DEF_REP, nullptr, m3::nextlog2<DEF_RBUF_SIZE>::val, DEF_RBUF_ORDER, 0
);

void RecvBuf::UpcallWorkItem::work() {
    DTU &dtu = DTU::get();
    RecvGate &upcall = RecvGate::upcall();
    epid_t ep = upcall.ep();
    DTU::Message *msg = dtu.fetch_msg(ep);
    if(msg) {
        LLOG(IPC, "Received msg @ " << (void*)msg << " over ep " << ep);
        RecvGate *rgate = msg->label ? reinterpret_cast<RecvGate*>(msg->label) : &upcall;
        GateIStream is(*rgate, msg);
        rgate->notify_all(is);
    }
}

void RecvBuf::RecvBufWorkItem::work() {
    DTU &dtu = DTU::get();
    assert(_ep != UNBOUND);
    DTU::Message *msg = dtu.fetch_msg(_ep);
    if(msg) {
        LLOG(IPC, "Received msg @ " << (void*)msg << " over ep " << _ep);
        RecvGate *gate = reinterpret_cast<RecvGate*>(msg->label);
        GateIStream is(*gate, msg);
        gate->notify_all(is);
    }
}

RecvBuf::RecvBuf(VPE &vpe, capsel_t cap, epid_t ep, void *buf, int order, int msgorder, uint flags)
    : ObjCap(RECV_BUF, cap, flags),
      _vpe(vpe),
      _buf(buf),
      _order(order),
      _ep(UNBOUND),
      _free(0),
      _workitem() {
    if(sel() != ObjCap::INVALID) {
        Errors::Code res = Syscalls::get().createrbuf(sel(), order, msgorder);
        if(res != Errors::NO_ERROR)
            PANIC("Creating recvbuf failed: " << Errors::to_string(res));
    }

    if(ep != UNBOUND)
        activate(ep);
}

RecvBuf RecvBuf::create(int order, int msgorder) {
    return create_for(VPE::self(), order, msgorder);
}

RecvBuf RecvBuf::create(capsel_t cap, int order, int msgorder) {
    return create_for(VPE::self(), cap, order, msgorder);
}

RecvBuf RecvBuf::create_for(VPE &vpe, int order, int msgorder) {
    return RecvBuf(vpe, VPE::self().alloc_cap(), UNBOUND, nullptr, order, msgorder, 0);
}

RecvBuf RecvBuf::create_for(VPE &vpe, capsel_t cap, int order, int msgorder) {
    return RecvBuf(vpe, cap, UNBOUND, nullptr, order, msgorder, KEEP_SEL);
}

RecvBuf RecvBuf::bind(capsel_t cap, int order) {
    return RecvBuf(VPE::self(), cap, order, KEEP_SEL | KEEP_CAP);
}

RecvBuf::~RecvBuf() {
    if(_free & FREE_BUF)
        free(_buf);
    deactivate();
}

void RecvBuf::activate() {
    if(_ep == UNBOUND) {
        epid_t ep = _vpe.alloc_ep();
        _free |= FREE_EP;
        activate(ep);
    }
}

void RecvBuf::activate(epid_t ep) {
    if(_ep == UNBOUND) {
        if(_buf == nullptr) {
            _buf = allocate(ep, 1UL << _order);
            _free |= FREE_BUF;
        }

        activate(ep, reinterpret_cast<uintptr_t>(_buf));
    }
}

void RecvBuf::activate(epid_t ep, uintptr_t addr) {
    assert(_ep == UNBOUND);

    _ep = ep;

    // first reserve the endpoint; we might need to invalidate it
    if(&_vpe == &VPE::self())
        EPMux::get().reserve(_ep);

#if defined(__t3__)
    // required for t3 because one can't write to these registers externally
    DTU::get().configure_recv(_ep, addr, order(), msgorder(), flags());
#endif

    if(sel() != ObjCap::INVALID) {
        Errors::Code res = Syscalls::get().activate(_vpe.sel(), _ep, sel(), addr);
        if(res != Errors::NO_ERROR)
            PANIC("Attaching recvbuf to " << _ep << " failed: " << Errors::to_string(res));
    }

    if(&_vpe == &VPE::self()) {
        // if we may receive messages from the endpoint, create a worker for it
        if(_ep == DTU::UPCALL_REP)
            env()->workloop()->add(new UpcallWorkItem(), true);
        else {
            if(_workitem == nullptr) {
                _workitem = new RecvBufWorkItem(_ep);
                bool permanent = _ep < DTU::FIRST_FREE_EP;
                env()->workloop()->add(_workitem, permanent);
            }
            else
                _workitem->ep(_ep);
        }
    }
}

void RecvBuf::disable() {
    if(_workitem) {
        env()->workloop()->remove(_workitem);
        delete _workitem;
        _workitem = nullptr;
    }
}

void RecvBuf::deactivate() {
    if(_free & FREE_EP)
        _vpe.free_ep(_ep);
    _ep = UNBOUND;

    disable();
}

}
