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

#include <m3/util/Sync.h>
#include <m3/Log.h>

#include "../../PEManager.h"
#include "../../RecvBufs.h"

using namespace m3;

void RecvBufs::configure(size_t coreid, size_t epid, RBuf &rbuf) {
    word_t regs[DTU::EPS_RCNT];
    memset(regs, 0, sizeof(regs));

    if(rbuf.flags & F_ATTACHED) {
        regs[DTU::EP_BUF_ADDR]       = rbuf.addr;
        regs[DTU::EP_BUF_ORDER]      = rbuf.order;
        regs[DTU::EP_BUF_MSGORDER]   = rbuf.msgorder;
        regs[DTU::EP_BUF_ROFF]       = 0;
        regs[DTU::EP_BUF_WOFF]       = 0;
        regs[DTU::EP_BUF_MSGCNT]     = 0;
        regs[DTU::EP_BUF_FLAGS]      = rbuf.flags & ~F_ATTACHED;
    }

    PEManager::get().vpe(coreid - APP_CORES).seps_gate().write_sync(
        regs, sizeof(regs), epid * sizeof(word_t) * DTU::EPS_RCNT);
}
