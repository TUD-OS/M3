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

#include "com/RecvBufs.h"
#include "pes/VPE.h"

namespace kernel {

uintptr_t RecvBufs::get_msgaddr(RBuf *rbuf, uintptr_t msgaddr) {
    // the message has to be within the receive buffer
    if(!(msgaddr >= rbuf->obj->addr && msgaddr < rbuf->obj->addr + rbuf->size()))
        return 0;

    // ensure that we start at a message boundary
    size_t idx = (msgaddr - rbuf->obj->addr) >> rbuf->obj->msgorder;
    return rbuf->obj->addr + (idx << rbuf->obj->msgorder);
}

m3::Errors::Code RecvBufs::get_header(VPE &vpe, const RBufObject *obj, uintptr_t &msgaddr, m3::DTU::Header &head) {
    RBuf *rbuf = get(obj);
    if(!rbuf)
        return m3::Errors::EP_INVALID;

    msgaddr = get_msgaddr(rbuf, msgaddr);
    if(!msgaddr)
        return m3::Errors::INV_ARGS;

    DTU::get().read_mem(vpe.desc(), msgaddr, &head, sizeof(head));
    return m3::Errors::NO_ERROR;
}

m3::Errors::Code RecvBufs::set_header(VPE &vpe, const RBufObject *obj, uintptr_t &msgaddr, const m3::DTU::Header &head) {
    RBuf *rbuf = get(obj);
    if(!rbuf)
        return m3::Errors::EP_INVALID;

    msgaddr = get_msgaddr(rbuf, msgaddr);
    if(!msgaddr)
        return m3::Errors::INV_ARGS;

    DTU::get().write_mem(vpe.desc(), msgaddr, &head, sizeof(head));
    return m3::Errors::NO_ERROR;
}

m3::Errors::Code RecvBufs::attach(VPE &vpe, const RBufObject *obj) {
    RBuf *rbuf = get(obj);
    if(rbuf)
        return m3::Errors::EXISTS;

    for(auto it = _rbufs.begin(); it != _rbufs.end(); ++it) {
        if(m3::Math::overlap(it->obj->addr, it->obj->addr + it->size(), obj->addr, obj->addr + it->size()))
            return m3::Errors::INV_ARGS;
    }

    rbuf = new RBuf(obj);
    rbuf->configure(vpe, true);
    _rbufs.append(rbuf);
    notify(obj);
    return m3::Errors::NO_ERROR;
}

void RecvBufs::detach(VPE &vpe, const RBufObject *obj) {
    RBuf *rbuf = get(obj);
    if(!rbuf)
        return;

    rbuf->configure(vpe, false);
    notify(obj);
    _rbufs.remove(rbuf);
    delete rbuf;
}

void RecvBufs::RBuf::configure(VPE &vpe, bool attach) {
    if(attach)
        vpe.config_rcv_ep(obj->ep, *obj);
    else
        vpe.invalidate_ep(obj->ep);
}

}
