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

extern size_t tempep;

namespace m3 {

void KVPE::start(int, char **, int) {
    // when exiting, the program will release one reference
    ref();
    activate_sysc_ep();

    // inject an IRQ
    uint64_t  val = 1;
    DTU::get().configure_mem(tempep, core(), IRQ_ADDR_EXTERN, sizeof(val));
    Sync::memory_barrier();
    DTU::get().write(tempep, &val, sizeof(val), 0);

    _state = RUNNING;
    LOG(VPES, "Started VPE '" << _name << "' [id=" << _id << "]");
}

void KVPE::activate_sysc_ep() {
    // configure syscall endpoint
    alignas(DTU_PKG_SIZE) word_t regs[4];
    uintptr_t addr = DTU::get().get_external_cmd_addr(
        DTU::SYSC_EP, EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD);
    DTU::get().config_remote_mem(regs, KERNEL_CORE, DTU::get().get_slot_addr(DTU::SYSC_EP),
        /* TODO 1 << SYSC_CREDIT_ORD*/0xFFFF, 0);
    DTU::get().configure_mem(tempep, core(), addr, 4 * sizeof(word_t));
    Sync::memory_barrier();
    DTU::get().write(tempep, regs, 4 * sizeof(word_t), 0);

    // configure label of syscall endpoint
    memset(regs, 0, sizeof(regs));
    addr = DTU::get().get_external_cmd_addr(DTU::SYSC_EP, OVERALL_SLOT_CFG);
    DTU::get().config_label(regs, reinterpret_cast<label_t>(&syscall_gate()));
    DTU::get().config_perm(regs,
        /* TODO atm, we need to give him all permissions */
        0xFFFFFFFF
        /*(1 << LOCAL_CFG_ADDRESS_FIFO_CMD) |
        (1 << TRANSFER_CFG_SIZE_STRIDE_REPEAT_CMD) |
        (1 << HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR) |
        (1 << FIRE_CMD) |
        (1 << DEBUG_CMD)*/
    );
    DTU::get().configure_mem(tempep, core(), addr, 2 * sizeof(word_t));
    Sync::memory_barrier();
    DTU::get().write(tempep, regs, 2 * sizeof(word_t), 0);

    // give him the core id
    alignas(DTU_PKG_SIZE) CoreConf conf;
    memset(&conf, 0, sizeof(conf));
    conf.coreid = core();
    DTU::get().configure_mem(tempep, core(), CONF_GLOBAL, sizeof(conf));
    Sync::memory_barrier();
    DTU::get().write(tempep, &conf, sizeof(conf), 0);
}

void KVPE::invalidate_eps() {
    alignas(DTU_PKG_SIZE) uint64_t regs[EXTERN_CFG_SIZE_CREDITS_CMD + 1] = {0};
    regs[OVERALL_SLOT_CFG] = (uint64_t)0xFFFFFFFF << 32;
    for(int i = 0; i < EP_COUNT; ++i) {
        uintptr_t addr = DTU::get().get_external_cmd_addr(i, 0);
        DTU::get().configure_mem(tempep, core(), addr, sizeof(regs));
        Sync::memory_barrier();
        DTU::get().write(tempep, regs, sizeof(regs), 0);
    }
}

Errors::Code KVPE::xchg_ep(size_t epid, MsgCapability *, MsgCapability *newcapobj) {
    alignas(DTU_PKG_SIZE) uint64_t regs[EXTERN_CFG_SIZE_CREDITS_CMD + 1] = {0};
    if(newcapobj) {
        uintptr_t remote_addr = (newcapobj->type & Capability::MEM) ? (newcapobj->obj->label & ~MemGate::RWX)
            : DTU::get().get_slot_addr(newcapobj->obj->epid);
        int addr_inc = (newcapobj->type & Capability::MEM) ? 1 : 0;
        regs[OVERALL_SLOT_CFG] = ((uint64_t)0xFFFFFFFF << 32) | newcapobj->obj->label;
        regs[EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD] =
            (uint64_t)((1 << 24) | (addr_inc << 16) | newcapobj->obj->core) << 32 | remote_addr;
        //uint credits = (newcapobj->obj->type & Capability::MEM) ? 0xFFFFFFFF : newcapobj->obj->credits;
        uint credits = 0xFFFFFFFF;
        regs[EXTERN_CFG_SIZE_CREDITS_CMD] = (uint64_t)credits << 32;
    }
    else
        regs[OVERALL_SLOT_CFG] = (uint64_t)0xFFFFFFFF << 32;

    uintptr_t addr = DTU::get().get_external_cmd_addr(epid, 0);
    DTU::get().configure_mem(tempep, core(), addr, sizeof(regs));
    Sync::memory_barrier();
    DTU::get().write(tempep, regs, sizeof(regs), 0);
    return Errors::NO_ERROR;
}

KVPE::~KVPE() {
    LOG(VPES, "Deleting VPE '" << _name << "' [id=" << _id << "]");
    SyscallHandler::get().remove_session(this);
    detach_rbufs();
    free_deps();
}

}
