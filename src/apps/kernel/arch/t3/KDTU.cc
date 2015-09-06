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

#include <m3/Common.h>
#include <m3/util/Sync.h>
#include <m3/DTU.h>

#include "../../KDTU.h"

namespace m3 {

void KDTU::wakeup(int core) {
    // inject an IRQ
    uint64_t val = 1;
    Sync::memory_barrier();
    write_mem(core, IRQ_ADDR_EXTERN, &val, sizeof(val));
}

void KDTU::deprivilege(int) {
    // nothing to do
}

void KDTU::invalidate_ep(int core, int ep) {
    alignas(DTU_PKG_SIZE) uint64_t regs[EXTERN_CFG_SIZE_CREDITS_CMD + 1];
    memset(regs, 0, sizeof(regs));
    regs[OVERALL_SLOT_CFG] = (uint64_t)0xFFFFFFFF << 32;

    Sync::memory_barrier();
    uintptr_t addr = DTU::get().get_external_cmd_addr(ep, 0);
    write_mem(core, addr, regs, sizeof(regs));
}

void KDTU::invalidate_eps(int core) {
    for(int i = 0; i < EP_COUNT; ++i)
        invalidate_ep(core, i);
}

void KDTU::config_recv_local(int ep, uintptr_t buf, uint order, uint msgorder, int flags) {
    DTU::get().configure_recv(ep, buf, order, msgorder, flags);
}

void KDTU::config_recv_remote(int, int, uintptr_t, uint, uint, bool) {
    // nothing to do; can only be done locally atm
}

void KDTU::config_send(void *e, label_t label, int dstcore, int dstep, size_t, word_t) {
    uint64_t *ep = reinterpret_cast<uint64_t*>(e);

    /* (1 << LOCAL_CFG_ADDRESS_FIFO_CMD) |
       (1 << TRANSFER_CFG_SIZE_STRIDE_REPEAT_CMD) |
       (1 << HEADER_CFG_REPLY_LABEL_SLOT_ENABLE_ADDR) |
       (1 << FIRE_CMD) |
       (1 << DEBUG_CMD) */
    /* TODO atm, we need to give him all permissions */

    ep[OVERALL_SLOT_CFG] = ((uint64_t)0xFFFFFFFF << 32) | label;
    ep[LOCAL_CFG_ADDRESS_FIFO_CMD] = 0;
    ep[TRANSFER_CFG_SIZE_STRIDE_REPEAT_CMD] = 0;
    ep[EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD] =
        (uint64_t)((1 << 24) | (0 << 16) | dstcore) << 32 | DTU::get().get_slot_addr(dstep);
    // TODO hand out the real credits
    ep[EXTERN_CFG_SIZE_CREDITS_CMD] = (uint64_t)0xFFFFFFFF << 32;
}

void KDTU::config_send_local(int ep, label_t label, int dstcore, int dstep,
        size_t msgsize, word_t credits) {
    config_send(DTU::get().get_cmd_addr(ep, 0), label, dstcore, dstep, msgsize, credits);
}

void KDTU::config_send_remote(int core, int ep, label_t label, int dstcore, int dstep,
        size_t msgsize, word_t credits) {
    alignas(DTU_PKG_SIZE) uint64_t regs[EXTERN_CFG_SIZE_CREDITS_CMD + 1];
    config_send(regs, label, dstcore, dstep, msgsize, credits);

    Sync::memory_barrier();
    uintptr_t epaddr = DTU::get().get_external_cmd_addr(ep, 0);
    write_mem(core, epaddr, regs, sizeof(regs));
}

void KDTU::config_mem(void *e, int dstcore, uintptr_t addr, size_t, int) {
    uint64_t *ep = reinterpret_cast<uint64_t*>(e);
    ep[OVERALL_SLOT_CFG] = ((uint64_t)0xFFFFFFFF << 32);
    ep[LOCAL_CFG_ADDRESS_FIFO_CMD] = 0;
    ep[TRANSFER_CFG_SIZE_STRIDE_REPEAT_CMD] = 0;
    ep[EXTERN_CFG_ADDRESS_MODULE_CHIP_CTA_INC_CMD] =
        (uint64_t)((1 << 24) | (1 << 16) | dstcore) << 32 | addr;
    // TODO hand out the real credits
    ep[EXTERN_CFG_SIZE_CREDITS_CMD] = (uint64_t)0xFFFFFFFF << 32;
}

void KDTU::config_mem_local(int ep, int coreid, uintptr_t addr, size_t size) {
    config_mem(DTU::get().get_cmd_addr(ep, 0), coreid, addr, size, MemGate::RW);
}

void KDTU::config_mem_remote(int core, int ep, int dstcore, uintptr_t addr, size_t size, int perms) {
    alignas(DTU_PKG_SIZE) uint64_t regs[EXTERN_CFG_SIZE_CREDITS_CMD + 1];
    config_mem(regs, dstcore, addr, size, perms);

    Sync::memory_barrier();
    uintptr_t epaddr = DTU::get().get_external_cmd_addr(ep, 0);
    write_mem(core, epaddr, regs, sizeof(regs));
}

void KDTU::reply_to(int core, int ep, int, word_t, label_t label, const void *msg, size_t size) {
    // TODO for some reason, we need to use a different EP here.
    static int tmpep = 0;
    if(tmpep == 0) {
        tmpep = VPE::self().alloc_ep();
        EPMux::get().reserve(tmpep);
    }
    config_send_local(tmpep, label, core, ep, size + DTU::HEADER_SIZE, size + DTU::HEADER_SIZE);
    DTU::get().send(tmpep, msg, size, 0, 0);
    DTU::get().wait_until_ready(tmpep);
}

void KDTU::write_mem(int core, uintptr_t addr, const void *data, size_t size) {
    config_mem_local(_ep, core, addr, size);
    DTU::get().write(_ep, data, size, 0);
}

}
