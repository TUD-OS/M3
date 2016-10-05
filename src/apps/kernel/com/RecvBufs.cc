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
    if(!(msgaddr >= rbuf->addr && msgaddr < rbuf->addr + rbuf->size()))
        return 0;

    // ensure that we start at a message boundary
    size_t idx = (msgaddr - rbuf->addr) >> rbuf->msgorder;
    return rbuf->addr + (idx << rbuf->msgorder);
}

m3::Errors::Code RecvBufs::get_header(VPE &vpe, epid_t epid, uintptr_t &msgaddr, m3::DTU::Header &head) {
    RBuf *rbuf = get(epid);
    if(!rbuf)
        return m3::Errors::EP_INVALID;

    msgaddr = get_msgaddr(rbuf, msgaddr);
    if(!msgaddr)
        return m3::Errors::INV_ARGS;

    DTU::get().read_mem(vpe.desc(), msgaddr, &head, sizeof(head));
    return m3::Errors::NO_ERROR;
}

m3::Errors::Code RecvBufs::set_header(VPE &vpe, epid_t epid, uintptr_t &msgaddr, const m3::DTU::Header &head) {
    RBuf *rbuf = get(epid);
    if(!rbuf)
        return m3::Errors::EP_INVALID;

    msgaddr = get_msgaddr(rbuf, msgaddr);
    if(!msgaddr)
        return m3::Errors::INV_ARGS;

    DTU::get().write_mem(vpe.desc(), msgaddr, &head, sizeof(head));
    return m3::Errors::NO_ERROR;
}

m3::Errors::Code RecvBufs::attach(VPE &vpe, epid_t epid, uintptr_t addr, int order, int msgorder, uint flags) {
    RBuf *rbuf = get(epid);
    if(rbuf)
        return m3::Errors::EXISTS;

    for(auto it = _rbufs.begin(); it != _rbufs.end(); ++it) {
        if(it->epid == epid)
            return m3::Errors::EXISTS;

        if(m3::Math::overlap(it->addr, it->addr + it->size(), addr, addr + it->size()))
            return m3::Errors::INV_ARGS;
    }

    rbuf = new RBuf(epid, addr, order, msgorder, flags);
    rbuf->configure(vpe, true);
    _rbufs.append(rbuf);
    notify(epid);
    return m3::Errors::NO_ERROR;
}

void RecvBufs::detach(VPE &vpe, epid_t epid) {
    RBuf *rbuf = get(epid);
    if(!rbuf)
        return;

    rbuf->configure(vpe, false);
    notify(epid);
    _rbufs.remove(rbuf);
    delete rbuf;
}

void RecvBufs::detach_all(VPE &vpe, epid_t except) {
    // TODO not nice
    for(epid_t i = 0; i < EP_COUNT; ++i) {
        if(i == except)
            continue;
        detach(vpe, i);
    }
}

void RecvBufs::RBuf::configure(VPE &vpe, bool attach) {
    if(attach)
        vpe.config_rcv_ep(epid, addr, order, msgorder, flags);
    else
        vpe.invalidate_ep(epid);
}

}
