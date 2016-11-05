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
    RGateObject rgate(SYSC_MSGSIZE_ORD, SYSC_MSGSIZE_ORD);
    rgate.vpe = VPEManager::MAX_VPES;
    rgate.addr = 1;  // has to be non-zero
    rgate.ep = m3::DTU::SYSC_SEP;
    rgate.add_ref(); // don't free this

    // configure syscall endpoint
    SGateObject mobj(&rgate, reinterpret_cast<label_t>(this), 1 << SYSC_MSGSIZE_ORD);
    config_snd_ep(m3::DTU::SYSC_SEP, mobj);

    // configure notify endpoint
    rgate.ep = m3::DTU::NOTIFY_SEP;
    rgate.msgorder = rgate.order = NOTIFY_MSGSIZE_ORD;
    mobj.credits = m3::DTU::CREDITS_UNLIM;
    config_snd_ep(m3::DTU::NOTIFY_SEP, mobj);

    // attach syscall receive endpoint
    rgate.order = m3::nextlog2<SYSC_RBUF_SIZE>::val;
    rgate.msgorder = SYSC_RBUF_ORDER;
    rgate.addr = Platform::def_recvbuf(pe());
    config_rcv_ep(m3::DTU::SYSC_REP, rgate);

    // attach upcall receive endpoint
    rgate.order = m3::nextlog2<UPCALL_RBUF_SIZE>::val;
    rgate.msgorder = UPCALL_RBUF_ORDER;
    rgate.addr += SYSC_RBUF_SIZE;
    config_rcv_ep(m3::DTU::UPCALL_REP, rgate);

    // attach default receive endpoint
    rgate.order = m3::nextlog2<DEF_RBUF_SIZE>::val;
    rgate.msgorder = DEF_RBUF_ORDER;
    rgate.addr += UPCALL_RBUF_SIZE;
    config_rcv_ep(m3::DTU::DEF_REP, rgate);
}

void VPE::activate_sysc_ep(void *) {
}

}
