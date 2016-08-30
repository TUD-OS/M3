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

#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"
#include "SyscallHandler.h"

namespace kernel {

void VPE::init() {
    // attach default receive endpoint
    UNUSED m3::Errors::Code res = rbufs().attach(
        *this, m3::DTU::DEF_RECVEP, Platform::def_recvbuf(core()), DEF_RCVBUF_ORDER, DEF_RCVBUF_ORDER, 0);
    assert(res == m3::Errors::NO_ERROR);

    // configure syscall endpoint
    config_snd_ep(m3::DTU::SYSC_EP, reinterpret_cast<label_t>(&syscall_gate()),
        Platform::kernel_pe(), Platform::kernel_pe(),
        m3::DTU::SYSC_EP, 1 << SYSC_CREDIT_ORD, 1 << SYSC_CREDIT_ORD);
}

void VPE::activate_sysc_ep() {
}

m3::Errors::Code VPE::xchg_ep(epid_t epid, MsgCapability *, MsgCapability *n) {
    KLOG(EPS, "Setting ep " << epid << " of VPE " << id() << " to " << (n ? n->sel() : -1));

    if(n) {
        if(n->type & Capability::MEM) {
            uintptr_t addr = n->obj->label & ~m3::KIF::Perm::RWX;
            int perm = n->obj->label & m3::KIF::Perm::RWX;
            config_mem_ep(epid, n->obj->core, n->obj->vpe, addr, n->obj->credits, perm);
        }
        else {
            // TODO we could use a logical ep id for receiving credits
            // TODO but we still need to make sure that one can't just activate the cap again to
            // immediately regain the credits
            // TODO we need the max-msg size here
            config_snd_ep(epid,
                n->obj->label, n->obj->core, n->obj->vpe, n->obj->epid,
                n->obj->credits, n->obj->credits);
        }
    }
    else
        invalidate_ep(epid);
    return m3::Errors::NO_ERROR;
}

VPE::~VPE() {
    KLOG(VPES, "Deleting VPE '" << _name << "' [id=" << id() << "]");
    free_reqs();
    _objcaps.revoke_all();
    _mapcaps.revoke_all();
    delete _as;
}

}
