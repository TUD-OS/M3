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

#include <base/Common.h>
#include <base/log/Kernel.h>
#include <base/util/Sync.h>
#include <base/DTU.h>

#include "mem/MainMemory.h"
#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

void DTU::do_set_vpeid(const VPEDesc &vpe, int newVPE) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t vpeId = newVPE;
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::VPE_ID), &vpeId, sizeof(vpeId));
}

void DTU::do_ext_cmd(const VPEDesc &vpe, m3::DTU::reg_t cmd) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t reg = cmd;
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::EXT_CMD), &reg, sizeof(reg));
}

void DTU::clear_pt(uintptr_t pt) {
    // clear the pagetable
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    size_t pe = m3::DTU::noc_to_pe(pt);
    uintptr_t addr = m3::DTU::noc_to_virt(pt);
    for(size_t i = 0; i < PAGE_SIZE / sizeof(buffer); ++i)
        write_mem(VPEDesc(pe, VPE::INVALID_ID), addr + i * sizeof(buffer), buffer, sizeof(buffer));
}

void DTU::init() {
    do_set_vpeid(VPEDesc(Platform::kernel_pe(), VPE::INVALID_ID), Platform::kernel_pe());
}

int DTU::log_to_phys(int pe) {
    return pe;
}

void DTU::deprivilege(int core) {
    // unset the privileged flag
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t status = 0;
    m3::Sync::compiler_barrier();
    write_mem(VPEDesc(core, VPE::INVALID_ID),
        m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::STATUS), &status, sizeof(status));
}

void DTU::get_regs_state(int core, m3::DTU::reg_state_t *state) {
    read_mem(VPEDesc(core, VPE::INVALID_ID), m3::DTU::BASE_ADDR, state, sizeof(*state));
}

void DTU::set_regs_state(const VPEDesc &vpe, m3::DTU::reg_state_t *state) {
    // FIXME
    m3::DTU::reg_t *regs = reinterpret_cast<m3::DTU::reg_t*>(state);
    regs[(int)m3::DTU::DtuRegs::EXT_CMD] = 0;
    regs[(int)m3::DTU::DtuRegs::VPE_ID] = vpe.id;
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::BASE_ADDR, state, sizeof(*state));
}

void DTU::set_vpeid(const VPEDesc &vpe) {
    // currently, the invalid ID is still set, so specify that
    do_set_vpeid(VPEDesc(vpe.core, VPE::INVALID_ID), vpe.id);
}

void DTU::unset_vpeid(const VPEDesc &vpe) {
    do_set_vpeid(vpe, VPE::INVALID_ID);
}

void DTU::wakeup(const VPEDesc &vpe) {
    // only programmable cores use libm3 and thus need the core id
    // TODO actually, libm3 should write that, not the kernel; but libm3 doesn't know the coreid atm
    if(Platform::pe(vpe.id).is_programmable()) {
        // write the core id to the PE
        uint64_t id = vpe.core;
        m3::Sync::compiler_barrier();
        write_mem(vpe, RT_START, &id, sizeof(id));
    }

    do_ext_cmd(vpe, static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::WAKEUP_CORE));
}

void DTU::suspend(const VPEDesc &vpe) {
    // invalidate TLB and cache
    do_ext_cmd(vpe, static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INV_TLB));
    do_ext_cmd(vpe, static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INV_CACHE));

    // disable paging
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t status = 0;
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::STATUS), &status, sizeof(status));
}

void DTU::injectIRQ(const VPEDesc &vpe) {
    do_ext_cmd(vpe, static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INJECT_IRQ) | (0x40 << 3));
}

void DTU::set_rw_barrier(const VPEDesc &vpe, uintptr_t addr) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t barrier = addr;
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::RW_BARRIER), &barrier, sizeof(barrier));
}

void DTU::config_pf_remote(const VPEDesc &vpe, uint64_t rootpt, int ep) {
    static_assert(static_cast<int>(m3::DTU::DtuRegs::STATUS) == 0, "STATUS wrong");
    static_assert(static_cast<int>(m3::DTU::DtuRegs::ROOT_PT) == 1, "ROOT_PT wrong");
    static_assert(static_cast<int>(m3::DTU::DtuRegs::PF_EP) == 2, "PF_EP wrong");

    if(!rootpt) {
        // TODO read the root pt from the core; the HW sets it atm for apps that are started at boot
        uintptr_t addr = m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::ROOT_PT);
        read_mem(vpe, addr, &rootpt, sizeof(rootpt));
    }
    else {
        clear_pt(rootpt);

        // insert recursive entry
        uintptr_t addr = m3::DTU::noc_to_virt(rootpt);
        m3::DTU::pte_t pte = rootpt | m3::DTU::PTE_RWX;
        write_mem(VPEDesc(m3::DTU::noc_to_pe(rootpt), VPE::INVALID_ID),
            addr + m3::DTU::PTE_REC_IDX * sizeof(pte), &pte, sizeof(pte));
    }

    alignas(DTU_PKG_SIZE) m3::DTU::reg_t dtuRegs[3];
    uint flags = ep != EP_COUNT ? static_cast<uint>(m3::DTU::StatusFlags::PAGEFAULTS) : 0;
    dtuRegs[static_cast<size_t>(m3::DTU::DtuRegs::STATUS)]  = flags;
    dtuRegs[static_cast<size_t>(m3::DTU::DtuRegs::ROOT_PT)] = rootpt;
    dtuRegs[static_cast<size_t>(m3::DTU::DtuRegs::PF_EP)]   = ep;
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::STATUS), dtuRegs, sizeof(dtuRegs));

    // invalidate TLB, because we have changed the root PT
    do_ext_cmd(vpe, static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INV_TLB));
}

bool DTU::create_pt(const VPEDesc &vpe, uintptr_t virt, uintptr_t pteAddr,
        m3::DTU::pte_t pte, int perm) {
    // create the pagetable on demand
    if(pte == 0) {
        // if we don't have a pagetable for that yet, unmapping is a noop
        if(perm == 0)
            return true;

        // TODO this is prelimilary
        MainMemory::Allocation alloc = MainMemory::get().allocate(PAGE_SIZE);
        assert(alloc);

        pte = m3::DTU::build_noc_addr(alloc.pe(), alloc.addr) | m3::DTU::PTE_RWX;
        KLOG(PTES, "PE" << vpe.core << ": lvl 1 PTE for "
            << m3::fmt(virt, "p") << ": " << m3::fmt(pte, "#0x", 16));
        m3::DTU::get().write(_ep, &pte, sizeof(pte), pteAddr, m3::DTU::CmdFlags::NOPF);
    }

    assert((pte & m3::DTU::PTE_IRWX) == m3::DTU::PTE_RWX);
    return false;
}

bool DTU::create_ptes(const VPEDesc &vpe, uintptr_t &virt, uintptr_t pteAddr, m3::DTU::pte_t pte,
        uintptr_t &phys, uint &pages, int perm) {
    // note that we can assume here that map_pages is always called for the same set of
    // pages. i.e., it is not possible that we map page 1 and 2 and afterwards remap
    // only page 1. this is because we call map_pages with MapCapability, which can't
    // be resized. thus, we know that a downgrade for the first, is a downgrade for all
    // and that an existing mapping for the first is an existing mapping for all.

    m3::DTU::pte_t npte = phys | perm | m3::DTU::PTE_I;
    if(npte == pte)
        return true;

    bool downgrade = ((pte & m3::DTU::PTE_RWX) & ~(npte & m3::DTU::PTE_RWX)) != 0;
    downgrade |= (pte & ~m3::DTU::PTE_IRWX) != phys;
    uintptr_t endpte = m3::Math::min(pteAddr + pages * sizeof(npte),
        m3::Math::round_up(pteAddr + sizeof(npte), PAGE_SIZE));

    uint count = (endpte - pteAddr) / sizeof(npte);
    assert(count > 0);
    pages -= count;
    phys += count << PAGE_BITS;

    while(pteAddr < endpte) {
        KLOG(PTES, "PE" << vpe.core << ": lvl 0 PTE for "
            << m3::fmt(virt, "p") << ": " << m3::fmt(npte, "#0x", 16));
        m3::DTU::get().write(_ep, &npte, sizeof(npte), pteAddr, m3::DTU::CmdFlags::NOPF);

        // permissions downgraded?
        if(downgrade) {
            // do that manually instead of with do_ext_cmd, because we don't want to reconfigure
            // the endpoint
            alignas(DTU_PKG_SIZE) m3::DTU::reg_t reg =
                static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INV_PAGE) | (virt << 3);
            m3::Sync::compiler_barrier();
            m3::DTU::get().write(_ep, &reg, sizeof(reg),
                m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::EXT_CMD), m3::DTU::CmdFlags::NOPF);
        }

        pteAddr += sizeof(npte);
        virt += PAGE_SIZE;
        npte += PAGE_SIZE;
    }
    return false;
}

static uintptr_t get_pte_addr(uintptr_t virt, int level) {
    static uintptr_t recMask =
        (static_cast<uintptr_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 2)) |
        (static_cast<uintptr_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 1)) |
        (static_cast<uintptr_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 0));

    // at first, just shift it accordingly.
    virt >>= PAGE_BITS + level * m3::DTU::LEVEL_BITS;
    virt <<= m3::DTU::PTE_BITS;

    // now put in one PTE_REC_IDX's for each loop that we need to take
    int shift = level + 1;
    uintptr_t remMask = (1UL << (PAGE_BITS + m3::DTU::LEVEL_BITS * (m3::DTU::LEVEL_CNT - shift))) - 1;
    virt |= recMask & ~remMask;

    // finally, make sure that we stay within the bounds for virtual addresses
    // this is because of recMask, that might actually have too many of those.
    virt &= (1UL << (m3::DTU::LEVEL_CNT * m3::DTU::LEVEL_BITS + PAGE_BITS)) - 1;
    return virt;
}

void DTU::map_pages(const VPEDesc &vpe, uintptr_t virt, uintptr_t phys, uint pages, int perm) {
    // configure the memory EP once and use it for all accesses
    config_mem_local(_ep, vpe.core, vpe.id, 0, 0xFFFFFFFFFFFFFFFF);
    while(pages > 0) {
        for(int level = m3::DTU::LEVEL_CNT - 1; level >= 0; --level) {
            uintptr_t pteAddr = get_pte_addr(virt, level);

            m3::DTU::pte_t pte;
            m3::DTU::get().read(_ep, &pte, sizeof(pte), pteAddr, m3::DTU::CmdFlags::NOPF);
            if(level > 0) {
                if(create_pt(vpe, virt, pteAddr, pte, perm))
                    return;
            }
            else {
                if(create_ptes(vpe, virt, pteAddr, pte, phys, pages, perm))
                    return;
            }
        }
    }
}

void DTU::unmap_pages(const VPEDesc &vpe, uintptr_t virt, uint pages) {
    map_pages(vpe, virt, 0, pages, 0);

    // TODO remove pagetables on demand
}

void DTU::invalidate_ep(const VPEDesc &vpe, int ep) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t e[m3::DTU::EP_REGS];
    memset(&e, 0, sizeof(e));
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void DTU::invalidate_eps(const VPEDesc &vpe, int first) {
    m3::DTU::reg_t *eps = new m3::DTU::reg_t[m3::DTU::EP_REGS * (EP_COUNT - first)];
    size_t total = sizeof(*eps) * m3::DTU::EP_REGS * (EP_COUNT - first);
    memset(eps, 0, total);
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::ep_regs_addr(first), eps, total);
    delete[] eps;
}

void DTU::config_recv(void *e, uintptr_t buf, uint order, uint msgorder, int) {
    m3::DTU::reg_t *ep = reinterpret_cast<m3::DTU::reg_t*>(e);
    m3::DTU::reg_t bufSize = static_cast<m3::DTU::reg_t>(1) << (order - msgorder);
    m3::DTU::reg_t msgSize = static_cast<m3::DTU::reg_t>(1) << msgorder;
    ep[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::RECEIVE) << 61) |
            ((msgSize & 0xFFFF) << 32) | ((bufSize & 0xFFFF) << 16) | 0;
    ep[1] = buf;
    ep[2] = 0;
}

void DTU::config_recv_local(int ep, uintptr_t buf, uint order, uint msgorder, int flags) {
    config_recv(reinterpret_cast<m3::DTU::reg_t*>(m3::DTU::ep_regs_addr(ep)),
        buf, order, msgorder, flags);
}

void DTU::config_recv_remote(const VPEDesc &vpe, int ep, uintptr_t buf, uint order, uint msgorder,
        int flags, bool valid) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t e[m3::DTU::EP_REGS];
    memset(&e, 0, sizeof(e));

    if(valid)
        config_recv(&e, buf, order, msgorder, flags);

    // write to PE
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void DTU::config_send(void *e, label_t label, int dstcore, int dstvpe, int dstep,
        size_t msgsize, word_t) {
    m3::DTU::reg_t *ep = reinterpret_cast<m3::DTU::reg_t*>(e);
    ep[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::SEND) << 61) |
            ((dstvpe & 0xFFFF) << 16) | (msgsize & 0xFFFF);
    // TODO hand out "unlimited" credits atm
    ep[1] = ((dstcore & 0xFF) << 24) | ((dstep & 0xFF) << 16) | m3::DTU::CREDITS_UNLIM;
    ep[2] = label;
}

void DTU::config_send_local(int ep, label_t label, int dstcore, int dstvpe, int dstep,
        size_t msgsize, word_t credits) {
    config_send(reinterpret_cast<m3::DTU::reg_t*>(m3::DTU::ep_regs_addr(ep)),
        label, dstcore, dstvpe, dstep, msgsize, credits);
}

void DTU::config_send_remote(const VPEDesc &vpe, int ep, label_t label, int dstcore, int dstvpe,
        int dstep, size_t msgsize, word_t credits) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t e[m3::DTU::EP_REGS];
    memset(&e, 0, sizeof(e));
    config_send(&e, label, dstcore, dstvpe, dstep, msgsize, credits);

    // write to PE
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void DTU::config_mem(void *e, int dstcore, int dstvpe, uintptr_t addr, size_t size, int perm) {
    m3::DTU::reg_t *ep = reinterpret_cast<m3::DTU::reg_t*>(e);
    ep[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::MEMORY) << 61) | (size & 0x1FFFFFFFFFFFFFFF);
    ep[1] = addr;
    ep[2] = ((dstvpe & 0xFFFF) << 12) | ((dstcore & 0xFF) << 4) | (perm & 0x7);
}

void DTU::config_mem_local(int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size) {
    config_mem(reinterpret_cast<m3::DTU::reg_t*>(m3::DTU::ep_regs_addr(ep)),
        dstcore, dstvpe, addr, size, m3::DTU::R | m3::DTU::W);
}

void DTU::config_mem_remote(const VPEDesc &vpe, int ep, int dstcore, int dstvpe, uintptr_t addr,
        size_t size, int perm) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t e[m3::DTU::EP_REGS];
    memset(&e, 0, sizeof(e));
    config_mem(&e, dstcore, dstvpe, addr, size, perm);

    // write to PE
    m3::Sync::compiler_barrier();
    write_mem(vpe, m3::DTU::ep_regs_addr(ep), &e, sizeof(e));
}

void DTU::send_to(const VPEDesc &vpe, int ep, label_t label, const void *msg, size_t size,
        label_t replylbl, int replyep) {
    config_send_local(_ep, label, vpe.core, vpe.id, ep, size + m3::DTU::HEADER_SIZE,
        size + m3::DTU::HEADER_SIZE);
    m3::DTU::get().send(_ep, msg, size, replylbl, replyep);
}

void DTU::reply_to(const VPEDesc &vpe, int ep, int, word_t, label_t label, const void *msg,
        size_t size) {
    config_send_local(_ep, label, vpe.core, vpe.id, ep, size + m3::DTU::HEADER_SIZE,
        size + m3::DTU::HEADER_SIZE);
    m3::DTU::get().send(_ep, msg, size, 0, 0);
}

void DTU::write_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t size) {
    config_mem_local(_ep, vpe.core, vpe.id, addr, size);
    // the kernel can never cause pagefaults with reads/writes
    m3::DTU::get().write(_ep, data, size, 0, m3::DTU::CmdFlags::NOPF);
}

void DTU::read_mem(const VPEDesc &vpe, uintptr_t addr, void *data, size_t size) {
    config_mem_local(_ep, vpe.core, vpe.id, addr, size);
    m3::DTU::get().read(_ep, data, size, 0, m3::DTU::CmdFlags::NOPF);
}

}
