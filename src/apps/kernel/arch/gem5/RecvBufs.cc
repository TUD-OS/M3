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

#include "../../RecvBufs.h"

extern int tempep;

namespace m3 {

void RecvBufs::configure(size_t coreid, size_t epid, RBuf &rbuf) {
    DTU::EpRegs ep;
    memset(&ep, 0, sizeof(ep));

    if(rbuf.flags & F_ATTACHED) {
        // default receive endpoint
        ep.bufAddr = rbuf.addr;
        ep.bufSize = static_cast<size_t>(1) << (rbuf.order - rbuf.msgorder);
        ep.bufMsgSize = static_cast<size_t>(1) << rbuf.msgorder;
        ep.bufMsgCnt = 0;
        ep.bufReadPtr = rbuf.addr;
        ep.bufWritePtr = rbuf.addr;
    }
    Sync::compiler_barrier();

    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::ep_regs(epid));
    DTU::get().configure_mem(tempep, coreid, dst, sizeof(ep));
    DTU::get().write(tempep, &ep, sizeof(ep), 0);
}

}
