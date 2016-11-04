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

#include <m3/com/EPMux.h>
#include <m3/com/RecvGate.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

#include <thread/ThreadManager.h>

namespace m3 {

INIT_PRIO_RECVBUF RecvGate RecvGate::_syscall (
#if defined(__host__) || defined(__gem5__)
    VPE::self(), ObjCap::INVALID, DTU::SYSC_REP, nullptr, m3::nextlog2<SYSC_RBUF_SIZE>::val, SYSC_RBUF_ORDER, 0
#else
    VPE::self(), ObjCap::INVALID, DTU::SYSC_REP, reinterpret_cast<void*>(DEF_RCVBUF),
        DEF_RCVBUF_MSGORDER, DEF_RCVBUF_MSGORDER, 0
#endif
);

INIT_PRIO_RECVBUF RecvGate RecvGate::_upcall (
    VPE::self(), ObjCap::INVALID, DTU::UPCALL_REP, nullptr, m3::nextlog2<UPCALL_RBUF_SIZE>::val, UPCALL_RBUF_ORDER, 0
);

INIT_PRIO_RECVBUF RecvGate RecvGate::_default (
    VPE::self(), ObjCap::INVALID, DTU::DEF_REP, nullptr, m3::nextlog2<DEF_RBUF_SIZE>::val, DEF_RBUF_ORDER, 0
);

void RecvGate::RecvGateWorkItem::work() {
    DTU::Message *msg = DTU::get().fetch_msg(_buf->ep());
    if(msg) {
        LLOG(IPC, "Received msg @ " << (void*)msg << " over ep " << _buf->ep());
        GateIStream is(*_buf, msg);
        _buf->_handler(is);
    }
}

RecvGate::RecvGate(VPE &vpe, capsel_t cap, epid_t ep, void *buf, int order, int msgorder, uint flags)
    : ObjCap(RECV_BUF, cap, flags),
      _vpe(vpe),
      _buf(buf),
      _order(order),
      _ep(UNBOUND),
      _free(0),
      _handler(),
      _workitem() {
    if(sel() != ObjCap::INVALID) {
        Errors::Code res = Syscalls::get().creatergate(sel(), order, msgorder);
        if(res != Errors::NO_ERROR)
            PANIC("Creating RecvGate failed: " << Errors::to_string(res));
    }

    if(ep != UNBOUND)
        activate(ep);
}

RecvGate RecvGate::create(int order, int msgorder) {
    return create_for(VPE::self(), order, msgorder);
}

RecvGate RecvGate::create(capsel_t cap, int order, int msgorder) {
    return create_for(VPE::self(), cap, order, msgorder);
}

RecvGate RecvGate::create_for(VPE &vpe, int order, int msgorder) {
    return RecvGate(vpe, VPE::self().alloc_cap(), UNBOUND, nullptr, order, msgorder, 0);
}

RecvGate RecvGate::create_for(VPE &vpe, capsel_t cap, int order, int msgorder) {
    return RecvGate(vpe, cap, UNBOUND, nullptr, order, msgorder, KEEP_SEL);
}

RecvGate RecvGate::bind(capsel_t cap, int order) {
    return RecvGate(VPE::self(), cap, order, KEEP_SEL | KEEP_CAP);
}

RecvGate::~RecvGate() {
    if(_free & FREE_BUF)
        free(_buf);
    deactivate();
}

void RecvGate::activate() {
    if(_ep == UNBOUND) {
        epid_t ep = _vpe.alloc_ep();
        _free |= FREE_EP;
        activate(ep);
    }
}

void RecvGate::activate(epid_t ep) {
    if(_ep == UNBOUND) {
        if(_buf == nullptr) {
            _buf = allocate(ep, 1UL << _order);
            _free |= FREE_BUF;
        }

        activate(ep, reinterpret_cast<uintptr_t>(_buf));
    }
}

void RecvGate::activate(epid_t ep, uintptr_t addr) {
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
            PANIC("Attaching RecvGate to " << _ep << " failed: " << Errors::to_string(res));
    }
}

void RecvGate::deactivate() {
    if(_free & FREE_EP)
        _vpe.free_ep(_ep);
    _ep = UNBOUND;

    stop();
}

void RecvGate::start(msghandler_t handler) {
    activate();

    assert(&_vpe == &VPE::self());
    assert(!_workitem);
    _handler = handler;

    bool permanent = _ep < DTU::FIRST_FREE_EP;
    _workitem = new RecvGateWorkItem(this);
    env()->workloop()->add(_workitem, permanent);
}

void RecvGate::stop() {
    if(_workitem) {
        env()->workloop()->remove(_workitem);
        delete _workitem;
        _workitem = nullptr;
    }
}

Errors::Code RecvGate::reply(const void *data, size_t len, size_t msgidx) {
    // TODO hack to fix the race-condition on T2. as soon as we've replied to the other PE, he
    // might send us another message, which we might miss if we ACK this message after we've got
    // another one. so, ACK it now since the reply marks the end of the handling anyway.
#if defined(__t2__)
    DTU::get().mark_read(ep(), msgidx);
#endif

    Errors::Code res = DTU::get().reply(ep(), const_cast<void*>(data), len, msgidx);

    if(EXPECT_FALSE(res == Errors::VPE_GONE)) {
        void *event = ThreadManager::get().get_wait_event();
        res = Syscalls::get().forwardreply(sel(), data, len, msgidx, event);

        // if this has been done, go to sleep and wait until the kernel sends us the upcall
        if(res == Errors::UPCALL_REPLY) {
            ThreadManager::get().wait_for(event);
            auto *msg = reinterpret_cast<const KIF::Upcall::Notify*>(
                ThreadManager::get().get_current_msg());
            res = static_cast<Errors::Code>(msg->error);
        }
    }

    return res;
}

Errors::Code RecvGate::wait(SendGate *sgate, const DTU::Message **msg) {
    activate();

    while(1) {
        *msg = DTU::get().fetch_msg(ep());
        if(*msg)
            return Errors::NO_ERROR;

        if(sgate && !DTU::get().is_valid(sgate->ep()))
            return Errors::EP_INVALID;

        // don't report idles if we wait for a syscall reply
        DTU::get().try_sleep(!sgate || sgate->ep() != m3::DTU::SYSC_SEP);
    }
    UNREACHED;
}

}
