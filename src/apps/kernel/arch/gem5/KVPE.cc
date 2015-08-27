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

using namespace m3;

extern int tempchan;

void KVPE::start(int, char **, int) {
    // when exiting, the program will release one reference
    ref();
    activate_sysc_chan();
    _state = RUNNING;
    LOG(VPES, "Started VPE '" << _name << "' [id=" << _id << "]");
}

void KVPE::activate_sysc_chan() {
    // write the config to the PE
    alignas(DTU_PKG_SIZE) CoreConf conf;
    conf.coreid = core();
    Sync::compiler_barrier();
    DTU::get().configure_mem(tempchan, core(), CONF_LOCAL, sizeof(conf));
    DTU::get().write(tempchan, &conf, sizeof(conf), 0);

    // init the syscall endpoint
    DTU::Endpoint ep;
    memset(&ep, 0, sizeof(ep));
    ep.mode = DTU::EpMode::TRANSMIT_MESSAGE;
    ep.credits = 1 << SYSC_CREDIT_ORD;
    ep.maxMessageSize = 1 << SYSC_CREDIT_ORD;
    ep.targetCoreId = KERNEL_CORE;
    ep.targetEpId = ChanMng::SYSC_CHAN;
    ep.label = reinterpret_cast<label_t>(&syscall_gate());
    Sync::compiler_barrier();
    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::get().get_ep(ChanMng::SYSC_CHAN));
    DTU::get().configure_mem(tempchan, core(), dst, sizeof(DTU::Endpoint));
    DTU::get().write(tempchan, &ep, sizeof(DTU::Endpoint), 0);
}

Errors::Code KVPE::xchg_chan(size_t cid, MsgCapability *, MsgCapability *newcapobj) {
    // TODO later we need to use cmpxchg here
    DTU::Endpoint ep;
    memset(&ep, 0, sizeof(ep));
    if(newcapobj) {
        ep.mode = (newcapobj->type & Capability::MEM)
            ? DTU::EpMode::READ_MEMORY : DTU::EpMode::TRANSMIT_MESSAGE;
    }
    ep.credits = newcapobj ? newcapobj->obj->credits : 0;
    ep.targetCoreId = newcapobj ? newcapobj->obj->core : 0;
    ep.targetEpId = newcapobj ? newcapobj->obj->chanid : 0;
    ep.label = newcapobj ? newcapobj->obj->label : 0;
    // TODO this is not correct
    ep.maxMessageSize = newcapobj ? newcapobj->obj->credits : 0;
    Sync::compiler_barrier();
    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::get().get_ep(cid));
    DTU::get().configure_mem(tempchan, core(), dst, sizeof(DTU::Endpoint));
    DTU::get().write(tempchan, &ep, sizeof(DTU::Endpoint), 0);
    return Errors::NO_ERROR;
}

KVPE::~KVPE() {
    LOG(VPES, "Deleting VPE '" << _name << "' [id=" << _id << "]");
    SyscallHandler::get().remove_session(this);
    detach_rbufs();
    free_deps();
}
