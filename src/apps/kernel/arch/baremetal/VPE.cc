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
    // configure syscall endpoint
    MsgObject mobj(reinterpret_cast<label_t>(&syscall_gate()),
        Platform::kernel_pe(), VPEManager::MAX_VPES, m3::DTU::SYSC_SEP,
        1 << SYSC_MSGSIZE_ORD, 1 << SYSC_MSGSIZE_ORD);
    config_snd_ep(m3::DTU::SYSC_SEP, mobj);

    // configure notify endpoint
    mobj.epid = m3::DTU::NOTIFY_SEP;
    mobj.msgsize = 1 << NOTIFY_MSGSIZE_ORD;
    mobj.credits = m3::DTU::CREDITS_UNLIM;
    config_snd_ep(m3::DTU::NOTIFY_SEP, mobj);

    // attach syscall receive endpoint
    UNUSED m3::Errors::Code res = rbufs().attach(*this, m3::DTU::SYSC_REP,
        Platform::def_recvbuf(pe()), m3::nextlog2<SYSC_RBUF_SIZE>::val, SYSC_RBUF_ORDER);
    assert(res == m3::Errors::NO_ERROR);

    // attach upcall receive endpoint
    res = rbufs().attach(*this, m3::DTU::UPCALL_REP,
        Platform::def_recvbuf(pe()) + SYSC_RBUF_SIZE,
        m3::nextlog2<UPCALL_RBUF_SIZE>::val, UPCALL_RBUF_ORDER);
    assert(res == m3::Errors::NO_ERROR);
}

void VPE::activate_sysc_ep(void *) {
}

VPE::~VPE() {
    KLOG(VPES, "Deleting VPE '" << _name << "' [id=" << id() << "]");
    _state = DEAD;
    free_reqs();
    _objcaps.revoke_all();
    _mapcaps.revoke_all();
    delete _as;
}

}
