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

#include <base/util/Sync.h>
#include <base/log/Kernel.h>

#include "pes/VPEManager.h"
#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"
#include "SyscallHandler.h"

namespace kernel {

void VPE::init() {
    RBufObject rbuf(SYSC_MSGSIZE_ORD, SYSC_MSGSIZE_ORD);
    rbuf.vpe = VPEManager::MAX_VPES;
    rbuf.addr = 1;  // has to be non-zero
    rbuf.ep = m3::DTU::SYSC_SEP;
    rbuf.add_ref(); // don't free this

    // configure syscall endpoint
    MsgObject mobj(&rbuf, reinterpret_cast<label_t>(this), 1 << SYSC_MSGSIZE_ORD);
    config_snd_ep(m3::DTU::SYSC_SEP, mobj);

    // configure notify endpoint
    rbuf.ep = m3::DTU::NOTIFY_SEP;
    rbuf.msgorder = rbuf.order = NOTIFY_MSGSIZE_ORD;
    mobj.credits = m3::DTU::CREDITS_UNLIM;
    config_snd_ep(m3::DTU::NOTIFY_SEP, mobj);

    // attach syscall receive endpoint
    rbuf.order = m3::nextlog2<SYSC_RBUF_SIZE>::val;
    rbuf.msgorder = SYSC_RBUF_ORDER;
    rbuf.addr = Platform::def_recvbuf(pe());
    config_rcv_ep(m3::DTU::SYSC_REP, rbuf);

    // attach upcall receive endpoint
    rbuf.order = m3::nextlog2<UPCALL_RBUF_SIZE>::val;
    rbuf.msgorder = UPCALL_RBUF_ORDER;
    rbuf.addr += SYSC_RBUF_SIZE;
    config_rcv_ep(m3::DTU::UPCALL_REP, rbuf);

    // attach default receive endpoint
    rbuf.order = m3::nextlog2<DEF_RBUF_SIZE>::val;
    rbuf.msgorder = DEF_RBUF_ORDER;
    rbuf.addr += UPCALL_RBUF_SIZE;
    config_rcv_ep(m3::DTU::DEF_REP, rbuf);
}

void VPE::activate_sysc_ep(void *) {
}

VPE::~VPE() {
    KLOG(VPES, "Deleting VPE '" << _name << "' [id=" << id() << "]");
    _state = DEAD;
    free_reqs();
    _objcaps.revoke_all();
    _mapcaps.revoke_all();
    // ensure that there are no syscalls for this VPE anymore
    DTU::get().drop_msgs(SyscallHandler::get().ep(), reinterpret_cast<label_t>(this));
    delete _as;
}

}
