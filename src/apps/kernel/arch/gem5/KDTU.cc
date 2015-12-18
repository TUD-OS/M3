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

#include <m3/Common.h>
#include <m3/util/Sync.h>
#include <m3/DTU.h>

#include "../../KDTU.h"

namespace m3 {

void KDTU::wakeup(int core) {
    // wakeup core
    alignas(DTU_PKG_SIZE) DTU::reg_t cmd = static_cast<DTU::reg_t>(DTU::ExtCmdOpCode::WAKEUP_CORE);
    Sync::compiler_barrier();
    write_mem(core, DTU::dtu_reg_addr(DTU::DtuRegs::EXT_CMD), &cmd, sizeof(cmd));
}

void KDTU::deprivilege(int core) {
    // unset the privileged flag
    alignas(DTU_PKG_SIZE) DTU::reg_t status = 0;
    Sync::compiler_barrier();
    write_mem(core, DTU::dtu_reg_addr(DTU::DtuRegs::STATUS), &status, sizeof(status));
}

void KDTU::config_pf_remote(int core, int ep, uint64_t rootpt) {
    static_assert(static_cast<int>(DTU::DtuRegs::STATUS) == 0, "STATUS wrong");
    static_assert(static_cast<int>(DTU::DtuRegs::ROOT_PT) == 1, "ROOT_PT wrong");
    static_assert(static_cast<int>(DTU::DtuRegs::PF_EP) == 2, "PF_EP wrong");

    // TODO read the root pt from the core; the HW sets it atm
    config_mem_local(_ep, core, DTU::dtu_reg_addr(DTU::DtuRegs::ROOT_PT), sizeof(rootpt));
    DTU::get().read(_ep, &rootpt, sizeof(rootpt), 0);

    alignas(DTU_PKG_SIZE) DTU::reg_t dtuRegs[3];
    dtuRegs[static_cast<size_t>(DTU::DtuRegs::STATUS)]  = DTU::StatusFlags::PAGEFAULTS;
    dtuRegs[static_cast<size_t>(DTU::DtuRegs::ROOT_PT)] = rootpt;
    dtuRegs[static_cast<size_t>(DTU::DtuRegs::PF_EP)]   = ep;
    Sync::compiler_barrier();
    write_mem(core, DTU::dtu_reg_addr(DTU::DtuRegs::STATUS), dtuRegs, sizeof(dtuRegs));
}

static uintptr_t get_pte_addr(uintptr_t virt, int level) {
    static uintptr_t recMask = (DTU::PTE_REC_IDX << (PAGE_BITS + DTU::LEVEL_BITS * 2)) |
                               (DTU::PTE_REC_IDX << (PAGE_BITS + DTU::LEVEL_BITS * 1)) |
                               (DTU::PTE_REC_IDX << (PAGE_BITS + DTU::LEVEL_BITS * 0));

    // at first, just shift it accordingly.
    virt >>= PAGE_BITS + level * DTU::LEVEL_BITS;
    virt <<= DTU::PTE_BITS;

    // now put in one PTE_REC_IDX's for each loop that we need to take
    int shift = level + 1;
    uintptr_t remMask = (1UL << (PAGE_BITS + DTU::LEVEL_BITS * (DTU::LEVEL_CNT - shift))) - 1;
    virt |= recMask & ~remMask;

    // finally, make sure that we stay within the bounds for virtual addresses
    // this is because of recMask, that might actually have too many of those.
    virt &= (1UL << (DTU::LEVEL_CNT * DTU::LEVEL_BITS + PAGE_BITS)) - 1;
    return virt;
}

void KDTU::map_page(int core, uintptr_t virt, uintptr_t phys, int perm) {
    // configure the memory EP once and use it for all accesses
    config_mem_local(_ep, core, 0, 0xFFFFFFFFFFFFFFFF);
    for(int level = DTU::LEVEL_CNT - 1; level >= 0; --level) {
        uintptr_t pteAddr = get_pte_addr(virt, level);
        DTU::pte_t pte;
        if(level > 0) {
            DTU::get().read(_ep, &pte, sizeof(pte), pteAddr);
            // TODO create the pagetable on demand
            assert((pte & DTU::PTE_IRWX) == DTU::PTE_RWX);
        }
        else {
            pte = phys | perm | DTU::PTE_I;
            DTU::get().write(_ep, &pte, sizeof(pte), pteAddr);
        }
    }
}

void KDTU::unmap_page(int core, uintptr_t virt) {
    map_page(core, virt, 0, 0);

    // TODO remove pagetables on demand

    // invalidate TLB entry
    alignas(DTU_PKG_SIZE) DTU::reg_t reg = static_cast<DTU::reg_t>(DTU::ExtCmdOpCode::INV_PAGE);
    reg |= virt << 2;
    Sync::compiler_barrier();
    write_mem(core, DTU::dtu_reg_addr(DTU::DtuRegs::EXT_CMD), &reg, sizeof(reg));
}

void KDTU::invalidate_ep(int core, int ep) {
    alignas(DTU_PKG_SIZE) DTU::reg_t e[DTU::EP_REGS];
    memset(&e, 0, sizeof(e));
    Sync::compiler_barrier();
    write_mem(core, DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void KDTU::invalidate_eps(int core) {
    DTU::reg_t *eps = new DTU::reg_t[DTU::EP_REGS * EP_COUNT];
    size_t total = sizeof(*eps) * DTU::EP_REGS * EP_COUNT;
    memset(eps, 0, total);
    Sync::compiler_barrier();
    write_mem(core, DTU::ep_regs_addr(0), eps, total);
    delete[] eps;
}

void KDTU::config_recv(void *e, uintptr_t buf, uint order, uint msgorder, int) {
    DTU::reg_t *ep = reinterpret_cast<DTU::reg_t*>(e);
    DTU::reg_t bufSize = static_cast<DTU::reg_t>(1) << (order - msgorder);
    DTU::reg_t msgSize = static_cast<DTU::reg_t>(1) << msgorder;
    ep[0] = (static_cast<DTU::reg_t>(DTU::EpType::RECEIVE) << 61) |
            ((msgSize & 0xFFFF) << 32) | ((bufSize & 0xFFFF) << 16) | 0;
    ep[1] = buf;
    ep[2] = 0;
}

void KDTU::config_recv_local(int ep, uintptr_t buf, uint order, uint msgorder, int flags) {
    config_recv(reinterpret_cast<DTU::reg_t*>(DTU::ep_regs_addr(ep)),
        buf, order, msgorder, flags);
}

void KDTU::config_recv_remote(int core, int ep, uintptr_t buf, uint order, uint msgorder, int flags,
        bool valid) {
    alignas(DTU_PKG_SIZE) DTU::reg_t e[DTU::EP_REGS];
    memset(&e, 0, sizeof(e));

    if(valid)
        config_recv(&e, buf, order, msgorder, flags);

    // write to PE
    Sync::compiler_barrier();
    write_mem(core, DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void KDTU::config_send(void *e, label_t label, int dstcore, int dstep,
        size_t msgsize, word_t) {
    DTU::reg_t *ep = reinterpret_cast<DTU::reg_t*>(e);
    ep[0] = (static_cast<DTU::reg_t>(DTU::EpType::SEND) << 61) | (msgsize & 0xFFFF);
    // TODO hand out "unlimited" credits atm
    ep[1] = ((dstcore & 0xFF) << 24) | ((dstep & 0xFF) << 16) | 0xFFFF;
    ep[2] = label;
}

void KDTU::config_send_local(int ep, label_t label, int dstcore, int dstep,
        size_t msgsize, word_t credits) {
    config_send(reinterpret_cast<DTU::reg_t*>(DTU::ep_regs_addr(ep)),
        label, dstcore, dstep, msgsize, credits);
}

void KDTU::config_send_remote(int core, int ep, label_t label, int dstcore, int dstep,
        size_t msgsize, word_t credits) {
    alignas(DTU_PKG_SIZE) DTU::reg_t e[DTU::EP_REGS];
    memset(&e, 0, sizeof(e));
    config_send(&e, label, dstcore, dstep, msgsize, credits);

    // write to PE
    Sync::compiler_barrier();
    write_mem(core, DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void KDTU::config_mem(void *e, int dstcore, uintptr_t addr, size_t size, int perm) {
    DTU::reg_t *ep = reinterpret_cast<DTU::reg_t*>(e);
    ep[0] = (static_cast<DTU::reg_t>(DTU::EpType::MEMORY) << 61) | (size & 0x1FFFFFFFFFFFFFFF);
    ep[1] = addr;
    ep[2] = ((dstcore & 0xFF) << 4) | (perm & 0x7);
}

void KDTU::config_mem_local(int ep, int coreid, uintptr_t addr, size_t size) {
    config_mem(reinterpret_cast<DTU::reg_t*>(DTU::ep_regs_addr(ep)),
        coreid, addr, size, DTU::R | DTU::W);
}

void KDTU::config_mem_remote(int core, int ep, int dstcore, uintptr_t addr, size_t size, int perm) {
    alignas(DTU_PKG_SIZE) DTU::reg_t e[DTU::EP_REGS];
    memset(&e, 0, sizeof(e));
    config_mem(&e, dstcore, addr, size, perm);

    // write to PE
    Sync::compiler_barrier();
    write_mem(core, DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void KDTU::reply_to(int core, int ep, int, word_t, label_t label, const void *msg, size_t size) {
    config_send_local(_ep, label, core, ep, size + DTU::HEADER_SIZE, size + DTU::HEADER_SIZE);
    DTU::get().send(_ep, msg, size, 0, 0);
}

void KDTU::write_mem(int core, uintptr_t addr, const void *data, size_t size) {
    config_mem_local(_ep, core, addr, size);
    DTU::get().write(_ep, data, size, 0);
}

}
