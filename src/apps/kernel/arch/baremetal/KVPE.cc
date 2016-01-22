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

#include <m3/util/Sync.h>
#include <m3/Log.h>

#include "../../PEManager.h"
#include "../../SyscallHandler.h"
#include "../../KVPE.h"
#include "../../KDTU.h"

namespace m3 {

void KVPE::start(int, char **, int) {
    // when exiting, the program will release one reference
    ref();
    activate_sysc_ep();

    KDTU::get().wakeup(core());

    _state = RUNNING;
    LOG(VPES, "Started VPE '" << _name << "' [id=" << _id << "]");
}

void KVPE::activate_sysc_ep() {
    // write the config to the PE
    alignas(DTU_PKG_SIZE) CoreConf conf;
    memset(&conf, 0, sizeof(conf));
    conf.coreid = core();
    Sync::compiler_barrier();
    KDTU::get().write_mem(core(), CONF_GLOBAL, &conf, sizeof(conf));

    // attach default receive endpoint
    UNUSED Errors::Code res = RecvBufs::attach(
        core(), DTU::DEF_RECVEP, DEF_RCVBUF, DEF_RCVBUF_ORDER, DEF_RCVBUF_ORDER, 0);
    assert(res == Errors::NO_ERROR);

    KDTU::get().config_send_remote(core(), DTU::SYSC_EP, reinterpret_cast<label_t>(&syscall_gate()),
        KERNEL_CORE, DTU::SYSC_EP, 1 << SYSC_CREDIT_ORD, 1 << SYSC_CREDIT_ORD);
}

Errors::Code KVPE::xchg_ep(size_t epid, MsgCapability *, MsgCapability *n) {
    if(n) {
        if(n->type & Capability::MEM) {
            uintptr_t addr = n->obj->label & ~MemGate::RWX;
            int perm = n->obj->label & MemGate::RWX;
            KDTU::get().config_mem_remote(core(), epid,
                n->obj->core, addr, n->obj->credits, perm);
        }
        else {
            // TODO we could use a logical ep id for receiving credits
            // TODO but we still need to make sure that one can't just activate the cap again to
            // immediately regain the credits
            // TODO we need the max-msg size here
            KDTU::get().config_send_remote(core(), epid,
                n->obj->label, n->obj->core, n->obj->epid, n->obj->credits, n->obj->credits);
        }
    }
    else
        KDTU::get().invalidate_ep(core(), epid);
    return Errors::NO_ERROR;
}

KVPE::~KVPE() {
    LOG(VPES, "Deleting VPE '" << _name << "' [id=" << _id << "]");
    SyscallHandler::get().remove_session(this);
    detach_rbufs();
    free_reqs();
}

}
