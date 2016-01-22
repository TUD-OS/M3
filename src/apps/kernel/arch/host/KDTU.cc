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

namespace m3 {

void KDTU::deprivilege(int) {
    // not supported
}

void KDTU::invalidate_eps(int core) {
    size_t total = DTU::EPS_RCNT * EP_COUNT;
    word_t *regs = new word_t[total];
    memset(regs, 0, total);
    PEManager::get().vpe(core - APP_CORES).seps_gate().write_sync(
        regs, total * sizeof(word_t), 0);
    delete[] regs;
}

void KDTU::config_recv(void *e, uintptr_t buf, uint order, uint msgorder, int flags) {
    word_t *regs = reinterpret_cast<word_t*>(e);
    regs[DTU::EP_BUF_ADDR]       = buf;
    regs[DTU::EP_BUF_ORDER]      = order;
    regs[DTU::EP_BUF_MSGORDER]   = msgorder;
    regs[DTU::EP_BUF_ROFF]       = 0;
    regs[DTU::EP_BUF_WOFF]       = 0;
    regs[DTU::EP_BUF_MSGCNT]     = 0;
    regs[DTU::EP_BUF_FLAGS]      = flags;
}

void KDTU::config_recv_local(int ep, uintptr_t buf, uint order, uint msgorder, int flags) {
    config_recv(DTU::get().ep_regs() + (ep * DTU::EPS_RCNT), buf, order, msgorder, flags);
}

void KDTU::config_recv_remote(int core, int ep, uintptr_t buf, uint order, uint msgorder, int flags,
        bool valid) {
    word_t regs[DTU::EPS_RCNT];
    memset(regs, 0, sizeof(regs));

    if(valid)
        config_recv(regs, buf, order, msgorder, flags);

    PEManager::get().vpe(core - APP_CORES).seps_gate().write_sync(
        regs, sizeof(regs), ep * sizeof(word_t) * DTU::EPS_RCNT);
}

void KDTU::reply_to(int core, int ep, int crdep, word_t credits, label_t label, const void *msg, size_t size) {
    DTU::get().configure(_ep, label, core, ep, size + DTU::HEADER_SIZE);
    DTU::get().sendcrd(_ep, crdep, credits);
    DTU::get().wait_until_ready(_ep);
    DTU::get().send(_ep, msg, size, 0, 0);
    DTU::get().wait_until_ready(_ep);
}

}
