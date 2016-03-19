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

#include "pes/PEManager.h"
#include "DTU.h"

namespace kernel {

void DTU::init() {
    // nothing to do
}

void DTU::set_vpeid(int, int) {
    // unsupported
}

void DTU::unset_vpeid(int, int) {
    // unsupported
}

void DTU::wakeup(VPE &) {
    // nothing to do
}

void DTU::suspend(VPE &) {
    // nothing to do
}

void DTU::injectIRQ(VPE &) {
    // unsupported
}

void DTU::deprivilege(int) {
    // unsupported
}

void DTU::invalidate_eps(VPE &vpe) {
    size_t total = m3::DTU::EPS_RCNT * EP_COUNT;
    word_t *regs = new word_t[total];
    memset(regs, 0, total);
    write_mem(vpe, reinterpret_cast<uintptr_t>(vpe.eps()), regs, total * sizeof(word_t));
    delete[] regs;
}

void DTU::config_pf_remote(VPE &, int) {
    // unsupported
}

void DTU::map_page(VPE &, uintptr_t, uintptr_t, int) {
    // unsupported
}

void DTU::unmap_page(VPE &, uintptr_t) {
    // unsupported
}

void DTU::config_send_local(int ep, label_t label, int dstcore, int, int dstep, size_t, word_t credits) {
    m3::DTU::get().configure(ep, label, dstcore, dstep, credits);
}

void DTU::config_send_remote(VPE &, int, label_t, int, int, int, size_t, word_t) {
    // nothing to do
}

void DTU::config_recv(void *e, uintptr_t buf, uint order, uint msgorder, int flags) {
    word_t *regs = reinterpret_cast<word_t*>(e);
    regs[m3::DTU::EP_BUF_ADDR]       = buf;
    regs[m3::DTU::EP_BUF_ORDER]      = order;
    regs[m3::DTU::EP_BUF_MSGORDER]   = msgorder;
    regs[m3::DTU::EP_BUF_ROFF]       = 0;
    regs[m3::DTU::EP_BUF_WOFF]       = 0;
    regs[m3::DTU::EP_BUF_MSGCNT]     = 0;
    regs[m3::DTU::EP_BUF_FLAGS]      = flags;
}

void DTU::config_recv_local(int ep, uintptr_t buf, uint order, uint msgorder, int flags) {
    config_recv(m3::DTU::get().ep_regs() + (ep * m3::DTU::EPS_RCNT), buf, order, msgorder, flags);
}

void DTU::config_recv_remote(VPE &vpe, int ep, uintptr_t buf, uint order, uint msgorder, int flags,
        bool valid) {
    word_t regs[m3::DTU::EPS_RCNT];
    memset(regs, 0, sizeof(regs));

    if(valid)
        config_recv(regs, buf, order, msgorder, flags);

    uintptr_t addr = reinterpret_cast<uintptr_t>(vpe.eps()) + ep * sizeof(word_t) * m3::DTU::EPS_RCNT;
    write_mem(vpe, addr, regs, sizeof(regs));
}

void DTU::send_to(VPE &vpe, int ep, label_t label, const void *msg, size_t size, label_t replylbl, int replyep) {
    m3::DTU::get().wait_until_ready(_ep);
    m3::DTU::get().configure(_ep, label, vpe.core(), ep, size + m3::DTU::HEADER_SIZE);
    m3::DTU::get().send(_ep, msg, size, replylbl, replyep);
    m3::DTU::get().wait_until_ready(_ep);
}

void DTU::reply_to(VPE &vpe, int ep, int crdep, word_t credits, label_t label, const void *msg, size_t size) {
    m3::DTU::get().wait_until_ready(_ep);
    m3::DTU::get().configure(_ep, label, vpe.core(), ep, size + m3::DTU::HEADER_SIZE);
    m3::DTU::get().sendcrd(_ep, crdep, credits);
    m3::DTU::get().wait_until_ready(_ep);
    m3::DTU::get().send(_ep, msg, size, 0, 0);
    m3::DTU::get().wait_until_ready(_ep);
}

void DTU::write_mem(VPE &vpe, uintptr_t addr, const void *data, size_t size) {
    m3::DTU::get().wait_until_ready(_ep);
    m3::DTU::get().configure(_ep, addr | m3::MemGate::RWX, vpe.core(), 0, size);
    m3::DTU::get().write(_ep, data, size, 0);
    m3::DTU::get().wait_until_ready(_ep);
}

void DTU::cmpxchg_mem(VPE &vpe, uintptr_t addr, const void *data, size_t datasize, size_t off, size_t size) {
    m3::DTU::get().wait_until_ready(_ep);
    m3::DTU::get().configure(_ep, (addr + off) | m3::MemGate::RWX, vpe.core(), 0, datasize);
    m3::DTU::get().cmpxchg(_ep, data, datasize, 0, size);
    m3::DTU::get().wait_until_ready(_ep);
    m3::DTU::get().wait_for_mem_cmd();
}

}
