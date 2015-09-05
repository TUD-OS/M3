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

namespace m3 {

void KVPE::start(int, char **, int) {
    // when exiting, the program will release one reference
    ref();
    activate_sysc_ep();

    // inject an IRQ
    uint64_t  val = 1;
    DTU::get().set_target(SLOT_NO, core(), IRQ_ADDR_EXTERN);
    Sync::memory_barrier();
    DTU::get().fire(SLOT_NO, DTU::WRITE, &val, sizeof(val));

    _state = RUNNING;
    LOG(VPES, "Started VPE '" << _name << "' [id=" << _id << "]");
}

void KVPE::activate_sysc_ep() {
    alignas(DTU_PKG_SIZE) CoreConf conf;
    memset(&conf, 0, sizeof(conf));
    conf.coreid = core();
    conf.eps[DTU::SYSC_EP].dstcore = KERNEL_CORE;
    conf.eps[DTU::SYSC_EP].dstep =DTU::SYSC_EP;
    conf.eps[DTU::SYSC_EP].label = reinterpret_cast<label_t>(&syscall_gate());
    DTU::get().set_target(SLOT_NO, core(), CONF_GLOBAL);
    Sync::memory_barrier();
    DTU::get().fire(SLOT_NO, DTU::WRITE, &conf, sizeof(conf));
}

void KVPE::invalidate_eps() {
    alignas(DTU_PKG_SIZE) CoreConf conf;
    memset(&conf, 0, sizeof(conf));
    conf.coreid = core();
    DTU::get().set_target(SLOT_NO, core(), CONF_GLOBAL);
    Sync::memory_barrier();
    DTU::get().fire(SLOT_NO, DTU::WRITE, &conf, sizeof(conf));
}

Errors::Code KVPE::xchg_ep(size_t epid, MsgCapability *, MsgCapability *newcapobj) {
    // TODO later we need to use cmpxchg here
    alignas(DTU_PKG_SIZE) EPConf conf;
    conf.dstcore = newcapobj ? newcapobj->obj->core : 0;
    conf.dstep = newcapobj ? newcapobj->obj->epid : 0;
    conf.label = newcapobj ? newcapobj->obj->label : label_t();
    conf.credits = newcapobj ? newcapobj->obj->credits : 0;
    DTU::get().set_target(SLOT_NO, core(), CONF_GLOBAL + offsetof(CoreConf, eps) + epid * sizeof(EPConf));
    Sync::memory_barrier();
    DTU::get().fire(SLOT_NO, DTU::WRITE, &conf, sizeof(conf));
    return Errors::NO_ERROR;
}

KVPE::~KVPE() {
    LOG(VPES, "Deleting VPE '" << _name << "' [id=" << _id << "]");
    SyscallHandler::get().remove_session(this);
    detach_rbufs();
    free_deps();
}

}
