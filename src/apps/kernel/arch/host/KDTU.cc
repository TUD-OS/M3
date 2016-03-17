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

#include <m3/Common.h>

#include "../../PEManager.h"
#include "../../KDTU.h"

namespace kernel {

void KDTU::init() {
    // nothing to do
}

void KDTU::set_vpeid(int, int) {
    // unsupported
}

void KDTU::unset_vpeid(int, int) {
    // unsupported
}

void KDTU::wakeup(KVPE &) {
    // nothing to do
}

void KDTU::suspend(KVPE &) {
    // nothing to do
}

void KDTU::injectIRQ(KVPE &) {
    // unsupported
}

void KDTU::deprivilege(int) {
    // unsupported
}

void KDTU::invalidate_eps(KVPE &vpe) {
    size_t total = m3::DTU::EPS_RCNT * EP_COUNT;
    word_t *regs = new word_t[total];
    memset(regs, 0, total);
    vpe.seps_gate().write_sync(regs, total * sizeof(word_t), 0);
    delete[] regs;
}

void KDTU::config_pf_remote(KVPE &, int) {
    // unsupported
}

void KDTU::map_page(KVPE &, uintptr_t, uintptr_t, int) {
    // unsupported
}

void KDTU::unmap_page(KVPE &, uintptr_t) {
    // unsupported
}

void KDTU::config_send_local(int ep, label_t label, int dstcore, int, int dstep, size_t, word_t credits) {
    m3::DTU::get().configure(ep, label, dstcore, dstep, credits);
}

void KDTU::config_send_remote(KVPE &, int, label_t, int, int, int, size_t, word_t) {
    // nothing to do
}

void KDTU::config_recv(void *e, uintptr_t buf, uint order, uint msgorder, int flags) {
    word_t *regs = reinterpret_cast<word_t*>(e);
    regs[m3::DTU::EP_BUF_ADDR]       = buf;
    regs[m3::DTU::EP_BUF_ORDER]      = order;
    regs[m3::DTU::EP_BUF_MSGORDER]   = msgorder;
    regs[m3::DTU::EP_BUF_ROFF]       = 0;
    regs[m3::DTU::EP_BUF_WOFF]       = 0;
    regs[m3::DTU::EP_BUF_MSGCNT]     = 0;
    regs[m3::DTU::EP_BUF_FLAGS]      = flags;
}

void KDTU::config_recv_local(int ep, uintptr_t buf, uint order, uint msgorder, int flags) {
    config_recv(m3::DTU::get().ep_regs() + (ep * m3::DTU::EPS_RCNT), buf, order, msgorder, flags);
}

void KDTU::config_recv_remote(KVPE &vpe, int ep, uintptr_t buf, uint order, uint msgorder, int flags,
        bool valid) {
    word_t regs[m3::DTU::EPS_RCNT];
    memset(regs, 0, sizeof(regs));

    if(valid)
        config_recv(regs, buf, order, msgorder, flags);

    vpe.seps_gate().write_sync(regs, sizeof(regs), ep * sizeof(word_t) * m3::DTU::EPS_RCNT);
}

void KDTU::reply_to(KVPE &vpe, int ep, int crdep, word_t credits, label_t label, const void *msg, size_t size) {
    m3::DTU::get().configure(_ep, label, vpe.core(), ep, size + m3::DTU::HEADER_SIZE);
    m3::DTU::get().sendcrd(_ep, crdep, credits);
    m3::DTU::get().wait_until_ready(_ep);
    m3::DTU::get().send(_ep, msg, size, 0, 0);
    m3::DTU::get().wait_until_ready(_ep);
}

}
