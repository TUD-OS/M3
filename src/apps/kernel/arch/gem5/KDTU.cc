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

#include "../../MainMemory.h"
#include "../../KDTU.h"
#include "../../KVPE.h"

namespace m3 {

void KDTU::do_set_vpeid(size_t core, int oldVPE, int newVPE) {
    alignas(DTU_PKG_SIZE) DTU::reg_t vpeId = newVPE;
    Sync::compiler_barrier();
    write_mem_at(core, oldVPE,
        DTU::dtu_reg_addr(DTU::DtuRegs::VPE_ID), &vpeId, sizeof(vpeId));
}

void KDTU::do_ext_cmd(KVPE &vpe, DTU::reg_t cmd) {
    alignas(DTU_PKG_SIZE) DTU::reg_t reg = cmd;
    Sync::compiler_barrier();
    write_mem(vpe, DTU::dtu_reg_addr(DTU::DtuRegs::EXT_CMD), &reg, sizeof(reg));
}

void KDTU::clear_pt(uintptr_t pt) {
    // clear the pagetable
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    uintptr_t addr = DTU::noc_to_virt(pt);
    for(size_t i = 0; i < PAGE_SIZE / sizeof(buffer); ++i)
        write_mem_at(MEMORY_CORE, 0, addr + i * sizeof(buffer), buffer, sizeof(buffer));
}

void KDTU::init() {
    do_set_vpeid(KERNEL_CORE, KVPE::INVALID_ID, 0);
}

void KDTU::deprivilege(int core) {
    // unset the privileged flag
    alignas(DTU_PKG_SIZE) DTU::reg_t status = 0;
    Sync::compiler_barrier();
    write_mem_at(core, KVPE::INVALID_ID,
        DTU::dtu_reg_addr(DTU::DtuRegs::STATUS), &status, sizeof(status));
}

void KDTU::set_vpeid(int core, int vpe) {
    // currently, the invalid ID is still set, so specify that
    do_set_vpeid(core, KVPE::INVALID_ID, vpe);
}

void KDTU::unset_vpeid(int core, int vpe) {
    do_set_vpeid(core, vpe, KVPE::INVALID_ID);
}

void KDTU::wakeup(KVPE &vpe) {
    // write the core id to the PE
    alignas(DTU_PKG_SIZE) CoreConf conf;
    conf.coreid = vpe.core();
    Sync::compiler_barrier();
    write_mem(vpe, CONF_GLOBAL, &conf, sizeof(conf));

    do_ext_cmd(vpe, static_cast<DTU::reg_t>(DTU::ExtCmdOpCode::WAKEUP_CORE));
}

void KDTU::suspend(KVPE &vpe) {
    // invalidate TLB and cache
    do_ext_cmd(vpe, static_cast<DTU::reg_t>(DTU::ExtCmdOpCode::INV_TLB));
    do_ext_cmd(vpe, static_cast<DTU::reg_t>(DTU::ExtCmdOpCode::INV_CACHE));

    // disable paging
    alignas(DTU_PKG_SIZE) DTU::reg_t status = 0;
    Sync::compiler_barrier();
    write_mem(vpe, DTU::dtu_reg_addr(DTU::DtuRegs::STATUS), &status, sizeof(status));
}

void KDTU::injectIRQ(KVPE &vpe) {
    do_ext_cmd(vpe, static_cast<DTU::reg_t>(DTU::ExtCmdOpCode::INJECT_IRQ) | (0x40 << 2));
}

void KDTU::config_pf_remote(KVPE &vpe, int ep) {
    static_assert(static_cast<int>(DTU::DtuRegs::STATUS) == 0, "STATUS wrong");
    static_assert(static_cast<int>(DTU::DtuRegs::ROOT_PT) == 1, "ROOT_PT wrong");
    static_assert(static_cast<int>(DTU::DtuRegs::PF_EP) == 2, "PF_EP wrong");

    uint64_t rootpt = vpe.address_space()->root_pt();
    if(!rootpt) {
        // TODO read the root pt from the core; the HW sets it atm for apps that are started at boot
        uintptr_t addr = DTU::dtu_reg_addr(DTU::DtuRegs::ROOT_PT);
        read_mem_at(vpe.core(), vpe.id(), addr, &rootpt, sizeof(rootpt));
    }
    else {
        clear_pt(rootpt);

        // insert recursive entry
        uintptr_t addr = DTU::noc_to_virt(rootpt);
        DTU::pte_t pte = rootpt | DTU::PTE_RWX;
        write_mem_at(MEMORY_CORE, 0, addr + PAGE_SIZE - sizeof(pte), &pte, sizeof(pte));
    }

    alignas(DTU_PKG_SIZE) DTU::reg_t dtuRegs[3];
    dtuRegs[static_cast<size_t>(DTU::DtuRegs::STATUS)]  = DTU::StatusFlags::PAGEFAULTS;
    dtuRegs[static_cast<size_t>(DTU::DtuRegs::ROOT_PT)] = rootpt;
    dtuRegs[static_cast<size_t>(DTU::DtuRegs::PF_EP)]   = ep;
    Sync::compiler_barrier();
    write_mem(vpe, DTU::dtu_reg_addr(DTU::DtuRegs::STATUS), dtuRegs, sizeof(dtuRegs));
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

void KDTU::map_page(KVPE &vpe, uintptr_t virt, uintptr_t phys, int perm) {
    // configure the memory EP once and use it for all accesses
    config_mem_local(_ep, vpe.core(), vpe.id(), 0, 0xFFFFFFFFFFFFFFFF);
    for(int level = DTU::LEVEL_CNT - 1; level >= 0; --level) {
        uintptr_t pteAddr = get_pte_addr(virt, level);
        DTU::pte_t pte;
        if(level > 0) {
            DTU::get().read(_ep, &pte, sizeof(pte), pteAddr);

            // create the pagetable on demand
            if(pte == 0) {
                // if we don't have a pagetable for that yet, unmapping is a noop
                if(perm == 0)
                    return;

                // TODO this is prelimilary
                uintptr_t addr = MainMemory::get().map().allocate(PAGE_SIZE);
                pte = DTU::build_noc_addr(MEMORY_CORE, addr) | DTU::PTE_RWX;
                LOG(PTES, "PE" << vpe.core() << ": lvl 1 PTE for "
                    << fmt(virt, "p") << ": " << fmt(pte, "#0x", 16));
                DTU::get().write(_ep, &pte, sizeof(pte), pteAddr);
            }

            assert((pte & DTU::PTE_IRWX) == DTU::PTE_RWX);
        }
        else {
            pte = phys | perm | DTU::PTE_I;
            LOG(PTES, "PE" << vpe.core() << ": lvl 0 PTE for "
                << fmt(virt, "p") << ": " << fmt(pte, "#0x", 16));
            DTU::get().write(_ep, &pte, sizeof(pte), pteAddr);
        }
    }
}

void KDTU::unmap_page(KVPE &vpe, uintptr_t virt) {
    map_page(vpe, virt, 0, 0);

    // TODO remove pagetables on demand

    // invalidate TLB entry
    do_ext_cmd(vpe, static_cast<DTU::reg_t>(DTU::ExtCmdOpCode::INV_PAGE) | (virt << 2));
}

void KDTU::invalidate_ep(KVPE &vpe, int ep) {
    alignas(DTU_PKG_SIZE) DTU::reg_t e[DTU::EP_REGS];
    memset(&e, 0, sizeof(e));
    Sync::compiler_barrier();
    write_mem(vpe, DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void KDTU::invalidate_eps(KVPE &vpe) {
    DTU::reg_t *eps = new DTU::reg_t[DTU::EP_REGS * EP_COUNT];
    size_t total = sizeof(*eps) * DTU::EP_REGS * EP_COUNT;
    memset(eps, 0, total);
    Sync::compiler_barrier();
    write_mem(vpe, DTU::ep_regs_addr(0), eps, total);
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

void KDTU::config_recv_remote(KVPE &vpe, int ep, uintptr_t buf, uint order, uint msgorder, int flags,
        bool valid) {
    alignas(DTU_PKG_SIZE) DTU::reg_t e[DTU::EP_REGS];
    memset(&e, 0, sizeof(e));

    if(valid)
        config_recv(&e, buf, order, msgorder, flags);

    // write to PE
    Sync::compiler_barrier();
    write_mem(vpe, DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void KDTU::config_send(void *e, label_t label, int dstcore, int dstvpe, int dstep,
        size_t msgsize, word_t) {
    DTU::reg_t *ep = reinterpret_cast<DTU::reg_t*>(e);
    ep[0] = (static_cast<DTU::reg_t>(DTU::EpType::SEND) << 61) |
            ((dstvpe & 0xFFFF) << 16) | (msgsize & 0xFFFF);
    // TODO hand out "unlimited" credits atm
    ep[1] = ((dstcore & 0xFF) << 24) | ((dstep & 0xFF) << 16) | 0xFFFF;
    ep[2] = label;
}

void KDTU::config_send_local(int ep, label_t label, int dstcore, int dstvpe, int dstep,
        size_t msgsize, word_t credits) {
    config_send(reinterpret_cast<DTU::reg_t*>(DTU::ep_regs_addr(ep)),
        label, dstcore, dstvpe, dstep, msgsize, credits);
}

void KDTU::config_send_remote(KVPE &vpe, int ep, label_t label, int dstcore, int dstvpe, int dstep,
        size_t msgsize, word_t credits) {
    alignas(DTU_PKG_SIZE) DTU::reg_t e[DTU::EP_REGS];
    memset(&e, 0, sizeof(e));
    config_send(&e, label, dstcore, dstvpe, dstep, msgsize, credits);

    // write to PE
    Sync::compiler_barrier();
    write_mem(vpe, DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void KDTU::config_mem(void *e, int dstcore, int dstvpe, uintptr_t addr, size_t size, int perm) {
    DTU::reg_t *ep = reinterpret_cast<DTU::reg_t*>(e);
    ep[0] = (static_cast<DTU::reg_t>(DTU::EpType::MEMORY) << 61) | (size & 0x1FFFFFFFFFFFFFFF);
    ep[1] = addr;
    ep[2] = ((dstvpe & 0xFFFF) << 12) | ((dstcore & 0xFF) << 4) | (perm & 0x7);
}

void KDTU::config_mem_local(int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size) {
    config_mem(reinterpret_cast<DTU::reg_t*>(DTU::ep_regs_addr(ep)),
        dstcore, dstvpe, addr, size, DTU::R | DTU::W);
}

void KDTU::config_mem_remote(KVPE &vpe, int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size, int perm) {
    alignas(DTU_PKG_SIZE) DTU::reg_t e[DTU::EP_REGS];
    memset(&e, 0, sizeof(e));
    config_mem(&e, dstcore, dstvpe, addr, size, perm);

    // write to PE
    Sync::compiler_barrier();
    write_mem(vpe, DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void KDTU::reply_to(KVPE &vpe, int ep, int, word_t, label_t label, const void *msg, size_t size) {
    config_send_local(_ep, label, vpe.core(), vpe.id(), ep, size + DTU::HEADER_SIZE, size + DTU::HEADER_SIZE);
    DTU::get().send(_ep, msg, size, 0, 0);
}

void KDTU::write_mem(KVPE &vpe, uintptr_t addr, const void *data, size_t size) {
    write_mem_at(vpe.core(), vpe.id(), addr, data, size);
}

void KDTU::write_mem_at(int core, int vpe, uintptr_t addr, const void *data, size_t size) {
    config_mem_local(_ep, core, vpe, addr, size);
    DTU::get().write(_ep, data, size, 0);
}

void KDTU::read_mem_at(int core, int vpe, uintptr_t addr, void *data, size_t size) {
    config_mem_local(_ep, core, vpe, addr, size);
    DTU::get().read(_ep, data, size, 0);
}

}
