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
#include "../../SyscallHandler.h"
#include "../../KVPE.h"

extern int tempep;

namespace m3 {

void KVPE::start(int, char **, int) {
    // when exiting, the program will release one reference
    ref();
    activate_sysc_ep();

    // wakeup core
    DTU::reg_t cmd = static_cast<DTU::reg_t>(DTU::CmdOpCode::WAKEUP_CORE);
    Sync::compiler_barrier();
    static_assert(offsetof(DTU::CmdRegs, command) == 0, "Command register is not at offset 0");
    DTU::get().configure_mem(tempep, core(), (uintptr_t)DTU::cmd_regs(), sizeof(cmd));
    DTU::get().write(tempep, &cmd, sizeof(cmd), 0);

    _state = RUNNING;
    LOG(VPES, "Started VPE '" << _name << "' [id=" << _id << "]");
}

void KVPE::activate_sysc_ep() {
    // write the config to the PE
    alignas(DTU_PKG_SIZE) CoreConf conf;
    conf.coreid = core();
    Sync::compiler_barrier();
    DTU::get().configure_mem(tempep, core(), CONF_LOCAL, sizeof(conf));
    DTU::get().write(tempep, &conf, sizeof(conf), 0);

    // attach default receive endpoint
    RecvBufs::attach(core(), DTU::DEF_RECVEP, DEF_RCVBUF, DEF_RCVBUF_ORDER, DEF_RCVBUF_ORDER, 0);

    // syscall endpoint
    DTU::EpRegs ep;
    memset(&ep, 0, sizeof(ep));
    ep.credits = 0xFFFFFFFF;// TODO 1 << SYSC_CREDIT_ORD;
    ep.maxMsgSize = 1 << SYSC_CREDIT_ORD;
    ep.targetCoreId = KERNEL_CORE;
    ep.targetEpId =DTU::SYSC_EP;
    ep.label = reinterpret_cast<label_t>(&syscall_gate());

    // write to PE
    Sync::compiler_barrier();
    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::ep_regs(DTU::SYSC_EP));
    DTU::get().configure_mem(tempep, core(), dst, sizeof(ep));
    DTU::get().write(tempep, &ep, sizeof(ep), 0);
}

void KVPE::invalidate_eps() {
    DTU::EpRegs *eps = new DTU::EpRegs[EP_COUNT];
    size_t total = sizeof(*eps) * EP_COUNT;
    memset(eps, 0, total);
    Sync::compiler_barrier();
    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::ep_regs(DTU::SYSC_EP));
    DTU::get().configure_mem(tempep, core(), dst, total);
    DTU::get().write(tempep, eps, total, 0);
    delete[] eps;
}

Errors::Code KVPE::xchg_ep(size_t epid, MsgCapability *, MsgCapability *newcapobj) {
    // TODO later we need to use cmpxchg here
    DTU::EpRegs ep;
    memset(&ep, 0, sizeof(ep));
    if(newcapobj) {
        ep.credits = 0xFFFFFFFF;// TODO newcapobj->obj->credits;
        ep.targetCoreId = newcapobj->obj->core;
        ep.targetEpId = newcapobj->obj->epid;
        ep.label = newcapobj->obj->label;
        // TODO this is not correct
        ep.maxMsgSize = newcapobj->obj->credits;
        ep.reqRemoteAddr = newcapobj->obj->label & ~MemGate::RWX;
        ep.reqRemoteSize = newcapobj->obj->credits;
        ep.reqFlags = newcapobj->obj->label & MemGate::RWX;
    }
    Sync::compiler_barrier();
    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::ep_regs(epid));
    DTU::get().configure_mem(tempep, core(), dst, sizeof(ep));
    DTU::get().write(tempep, &ep, sizeof(ep), 0);
    return Errors::NO_ERROR;
}

KVPE::~KVPE() {
    LOG(VPES, "Deleting VPE '" << _name << "' [id=" << _id << "]");
    SyscallHandler::get().remove_session(this);
    detach_rbufs();
    free_deps();
}

}
