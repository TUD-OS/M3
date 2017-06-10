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
#include <base/util/Math.h>
#include <base/CPU.h>
#include <base/DTU.h>

#include "mem/MainMemory.h"
#include "pes/VPEManager.h"
#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

static char buffer[4096];

void DTU::do_set_vpeid(const VPEDesc &vpe, vpeid_t nid) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t vpeId = nid;
    m3::CPU::compiler_barrier();
    write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::VPE_ID), &vpeId, sizeof(vpeId));
}

void DTU::do_ext_cmd(const VPEDesc &vpe, m3::DTU::reg_t cmd) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t reg = cmd;
    m3::CPU::compiler_barrier();
    write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::EXT_CMD), &reg, sizeof(reg));
}

void DTU::clear_pt(gaddr_t pt) {
    // clear the pagetable
    memset(buffer, 0, sizeof(buffer));
    peid_t pe = m3::DTU::gaddr_to_pe(pt);
    uintptr_t addr = m3::DTU::gaddr_to_virt(pt);
    for(size_t i = 0; i < PAGE_SIZE / sizeof(buffer); ++i)
        write_mem(VPEDesc(pe, VPE::INVALID_ID), addr + i * sizeof(buffer), buffer, sizeof(buffer));
}

void DTU::init() {
    do_set_vpeid(VPEDesc(Platform::kernel_pe(), VPE::INVALID_ID), VPEManager::MAX_VPES);
}

peid_t DTU::log_to_phys(peid_t pe) {
    return pe;
}

void DTU::deprivilege(peid_t pe) {
    // unset the privileged flag
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t features = 0;
    m3::CPU::compiler_barrier();
    write_mem(VPEDesc(pe, VPE::INVALID_ID),
        m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::FEATURES), &features, sizeof(features));
}

cycles_t DTU::get_time() {
    return m3::DTU::get().tsc();
}

void DTU::set_vpeid(const VPEDesc &vpe) {
    do_set_vpeid(VPEDesc(vpe.pe, VPE::INVALID_ID), vpe.id);
}

void DTU::unset_vpeid(const VPEDesc &vpe) {
    do_set_vpeid(vpe, VPE::INVALID_ID);
}

void DTU::wakeup(const VPEDesc &vpe) {
    do_ext_cmd(vpe, static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::WAKEUP_CORE));
}

void DTU::injectIRQ(const VPEDesc &vpe) {
    do_ext_cmd(vpe, static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INJECT_IRQ));
}

void DTU::mmu_cmd_remote(const VPEDesc &vpe, m3::DTU::reg_t arg) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t regs[2];
    regs[0] = static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INJECT_IRQ);
    regs[1] = arg;
    m3::CPU::compiler_barrier();
    write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::EXT_CMD), regs, sizeof(regs));

    // wait until the remote core sends us an ACK (writes 0 to EXT_ARG)
    m3::DTU::reg_t extarg = 1;
    uintptr_t extarg_addr = m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::EXT_ARG);
    while(extarg != 0)
        read_mem(vpe, extarg_addr, &extarg, sizeof(extarg));
}

void DTU::set_rootpt_remote(const VPEDesc &vpe, gaddr_t rootpt) {
    assert(Platform::pe(vpe.pe).has_mmu());
    mmu_cmd_remote(vpe, rootpt | m3::DTU::ExtPFCmdOpCode::SET_ROOTPT);
}

void DTU::invlpg_remote(const VPEDesc &vpe, uintptr_t virt) {
    virt &= ~static_cast<uintptr_t>(PAGE_MASK);
    if(Platform::pe(vpe.pe).has_mmu())
        mmu_cmd_remote(vpe, virt | m3::DTU::ExtPFCmdOpCode::INV_PAGE);
    do_ext_cmd(vpe, static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INV_PAGE) | (virt << 3));
}

typedef uint64_t mmu_pte_t;

static const mmu_pte_t X86_PTE_PRESENT  = 0x1;
static const mmu_pte_t X86_PTE_WRITE    = 0x2;
static const mmu_pte_t X86_PTE_USER     = 0x4;
static const mmu_pte_t X86_PTE_UNCACHED = 0x10;
static const mmu_pte_t X86_PTE_NOEXEC   = 1ULL << 63;

static mmu_pte_t to_mmu_pte(peid_t pe, m3::DTU::pte_t pte) {
    // the current implementation is based on some equal properties of MMU and DTU paging
    static_assert(sizeof(mmu_pte_t) == sizeof(m3::DTU::pte_t), "MMU and DTU PTEs incompatible");
    static_assert(m3::DTU::LEVEL_CNT == 4, "MMU and DTU PTEs incompatible: levels != 4");
    static_assert(PAGE_SIZE == 4096, "MMU and DTU PTEs incompatible: pagesize != 4k");
    static_assert(m3::DTU::LEVEL_BITS == 9, "MMU and DTU PTEs incompatible: level bits != 9");

    if(Platform::pe(pe).has_dtuvm())
        return pte;

    mmu_pte_t res = pte & ~static_cast<m3::DTU::pte_t>(PAGE_MASK);
    if(pte & m3::DTU::PTE_RWX)
        res |= X86_PTE_PRESENT;
    if(pte & m3::DTU::PTE_W)
        res |= X86_PTE_WRITE;
    if(pte & m3::DTU::PTE_I)
        res |= X86_PTE_USER;
    if(pte & m3::DTU::PTE_UNCACHED)
        res |= X86_PTE_UNCACHED;
    if(~pte & m3::DTU::PTE_X)
        res |= X86_PTE_NOEXEC;
    return res;
}

static m3::DTU::pte_t to_dtu_pte(peid_t pe, mmu_pte_t pte) {
    if(Platform::pe(pe).has_dtuvm() || pte == 0)
        return pte;

    m3::DTU::pte_t res = pte & ~static_cast<m3::DTU::pte_t>(PAGE_MASK);
    if(pte & X86_PTE_PRESENT)
        res |= m3::DTU::PTE_R;
    if(pte & X86_PTE_WRITE)
        res |= m3::DTU::PTE_W;
    if(pte & X86_PTE_USER)
        res |= m3::DTU::PTE_I;
    if(~pte & X86_PTE_NOEXEC)
        res |= m3::DTU::PTE_X;
    return res;
}

void DTU::config_pf_remote(const VPEDesc &vpe, gaddr_t rootpt, epid_t sep, epid_t rep) {
    // insert recursive entry
    uintptr_t addr = m3::DTU::gaddr_to_virt(rootpt);
    m3::DTU::pte_t pte = to_mmu_pte(vpe.pe, rootpt | m3::DTU::PTE_RWX);
    write_mem(VPEDesc(m3::DTU::gaddr_to_pe(rootpt), VPE::INVALID_ID),
        addr + m3::DTU::PTE_REC_IDX * sizeof(pte), &pte, sizeof(pte));

    if(Platform::pe(vpe.pe).has_dtuvm()) {
        static_assert(static_cast<int>(m3::DTU::DtuRegs::FEATURES) == 0, "FEATURES wrong");
        static_assert(static_cast<int>(m3::DTU::DtuRegs::ROOT_PT) == 1, "ROOT_PT wrong");
        static_assert(static_cast<int>(m3::DTU::DtuRegs::PF_EP) == 2, "PF_EP wrong");

        // init DTU registers
        alignas(DTU_PKG_SIZE) m3::DTU::reg_t regs[3];
        uint features = 0;
        if(sep != static_cast<epid_t>(-1))
            features = static_cast<uint>(m3::DTU::StatusFlags::PAGEFAULTS);
        regs[static_cast<size_t>(m3::DTU::DtuRegs::FEATURES)] = features;
        regs[static_cast<size_t>(m3::DTU::DtuRegs::ROOT_PT)] = rootpt;
        regs[static_cast<size_t>(m3::DTU::DtuRegs::PF_EP)] = sep | (rep << 8);
        m3::CPU::compiler_barrier();
        write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::FEATURES), regs, sizeof(regs));
    }
    else {
        static_assert(static_cast<int>(m3::DTU::DtuRegs::EXT_ARG) ==
                      static_cast<int>(m3::DTU::DtuRegs::EXT_CMD) + 1, "EXT_ARG wrong");

        alignas(DTU_PKG_SIZE) m3::DTU::reg_t reg = sep | (rep << 8);
        m3::CPU::compiler_barrier();
        write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::PF_EP), &reg, sizeof(reg));

        set_rootpt_remote(vpe, rootpt);
    }

    // invalidate TLB, because we have changed the root PT
    do_ext_cmd(vpe, static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INV_TLB));
}

bool DTU::create_pt(const VPEDesc &vpe, vpeid_t vpeid, uintptr_t virt, uintptr_t pteAddr,
        m3::DTU::pte_t pte, int perm, int level) {
    // create the pagetable on demand
    if(pte == 0) {
        // if we don't have a pagetable for that yet, unmapping is a noop
        if(perm == 0)
            return true;

        // TODO this is prelimilary
        MainMemory::Allocation alloc = MainMemory::get().allocate(PAGE_SIZE, PAGE_SIZE);
        assert(alloc);

        // clear PT
        pte = m3::DTU::build_gaddr(alloc.pe(), alloc.addr);
        clear_pt(pte);

        // insert PTE
        pte |= m3::DTU::PTE_IRWX;
        pte = to_mmu_pte(vpe.pe, pte);
        const size_t ptsize = (1UL << (m3::DTU::LEVEL_BITS * level)) * PAGE_SIZE;
        KLOG(PTES, "VPE" << vpeid << ": lvl " << level << " PTE for "
            << m3::fmt(virt & ~(ptsize - 1), "p") << ": " << m3::fmt(pte, "#0x", 16));
        write_mem(vpe, pteAddr, &pte, sizeof(pte));
    }
    return false;
}

bool DTU::create_ptes(const VPEDesc &vpe, vpeid_t vpeid, uintptr_t &virt, uintptr_t pteAddr,
        m3::DTU::pte_t pte, gaddr_t &phys, uint &pages, int perm) {
    // note that we can assume here that map_pages is always called for the same set of
    // pages. i.e., it is not possible that we map page 1 and 2 and afterwards remap
    // only page 1. this is because we call map_pages with MapCapability, which can't
    // be resized. thus, we know that a downgrade for the first, is a downgrade for all
    // and that an existing mapping for the first is an existing mapping for all.

    m3::DTU::pte_t pteDTU = to_dtu_pte(vpe.pe, pte);
    m3::DTU::pte_t npte = phys | static_cast<uint>(perm) | m3::DTU::PTE_I;
    if(npte == pteDTU)
        return true;

    bool downgrade = false;
    // do not invalidate pages if we are writing to a memory PE
    if((pteDTU & m3::DTU::PTE_RWX) && Platform::pe(vpe.pe).has_virtmem())
        downgrade = ((pteDTU & m3::DTU::PTE_RWX) & (~npte & m3::DTU::PTE_RWX)) != 0;

    uintptr_t endpte = m3::Math::min(pteAddr + pages * sizeof(npte),
        m3::Math::round_up(pteAddr + sizeof(npte), PAGE_SIZE));

    uint count = (endpte - pteAddr) / sizeof(npte);
    assert(count > 0);
    pages -= count;
    phys += count << PAGE_BITS;

    npte = to_mmu_pte(vpe.pe, npte);
    while(pteAddr < endpte) {
        size_t i = 0;
        uintptr_t startAddr = pteAddr;
        m3::DTU::pte_t buf[16];
        for(; pteAddr < endpte && i < ARRAY_SIZE(buf); ++i) {
            KLOG(PTES, "VPE" << vpeid << ": lvl 0 PTE for "
                << m3::fmt(virt, "p") << ": " << m3::fmt(npte, "#0x", 16)
                << (downgrade ? " (invalidating)" : ""));

            buf[i] = npte;

            pteAddr += sizeof(npte);
            virt += PAGE_SIZE;
            npte += PAGE_SIZE;
        }

        write_mem(vpe, startAddr, buf, i * sizeof(buf[0]));

        if(downgrade) {
            for(uintptr_t vaddr = virt - i * PAGE_SIZE; vaddr < virt; vaddr += PAGE_SIZE)
                invlpg_remote(vpe, vaddr);
        }
    }
    return false;
}

static uintptr_t get_pte_addr(uintptr_t virt, int level) {
    static uintptr_t recMask =
        (static_cast<uintptr_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 3)) |
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

uintptr_t DTU::get_pte_addr_mem(const VPEDesc &vpe, gaddr_t root, uintptr_t virt, int level) {
    uintptr_t pt = m3::DTU::gaddr_to_virt(root);
    for(int l = m3::DTU::LEVEL_CNT - 1; l >= 0; --l) {
        size_t idx = (virt >> (PAGE_BITS + m3::DTU::LEVEL_BITS * l)) & m3::DTU::LEVEL_MASK;
        pt += idx * m3::DTU::PTE_SIZE;

        if(level == l)
            return pt;

        m3::DTU::pte_t pte;
        read_mem(vpe, pt, &pte, sizeof(pte));

        pt = m3::DTU::gaddr_to_virt(pte & ~PAGE_MASK);
    }

    UNREACHED;
}

void DTU::map_pages(const VPEDesc &vpe, uintptr_t virt, gaddr_t phys, uint pages, int perm) {
    bool running = vpe.pe == Platform::kernel_pe() ||
        VPEManager::get().vpe(vpe.id).state() == VPE::RUNNING;

    VPEDesc rvpe(vpe);
    gaddr_t root = 0;
    if(!running) {
        VPE &v = VPEManager::get().vpe(vpe.id);
        // TODO we currently assume that all PTEs are in the same mem PE as the root PT
        peid_t pe = m3::DTU::gaddr_to_pe(v.address_space()->root_pt());
        root = v.address_space()->root_pt();
        rvpe = VPEDesc(pe, VPE::INVALID_ID);
    }

    while(pages > 0) {
        for(int level = m3::DTU::LEVEL_CNT - 1; level >= 0; --level) {
            uintptr_t pteAddr;
            if(!running)
                pteAddr = get_pte_addr_mem(rvpe, root, virt, level);
            else
                pteAddr = get_pte_addr(virt, level);

            m3::DTU::pte_t pte;
            read_mem(rvpe, pteAddr, &pte, sizeof(pte));
            if(level > 0) {
                if(create_pt(rvpe, vpe.id, virt, pteAddr, pte, perm, level))
                    return;
            }
            else {
                if(create_ptes(rvpe, vpe.id, virt, pteAddr, pte, phys, pages, perm))
                    return;
            }
        }
    }
}

void DTU::unmap_pages(const VPEDesc &vpe, uintptr_t virt, uint pages) {
    // don't do anything if the VPE is already dead
    if(vpe.pe != Platform::kernel_pe() && VPEManager::get().vpe(vpe.id).state() == VPE::DEAD)
        return;

    map_pages(vpe, virt, 0, pages, 0);
}

void DTU::remove_pts_rec(vpeid_t vpe, gaddr_t pt, uintptr_t virt, int level) {
    static_assert(sizeof(buffer) >= PAGE_SIZE, "Buffer smaller than a page");

    // load entire page table
    peid_t pe = m3::DTU::gaddr_to_pe(pt);
    VPEDesc memvpe(pe, VPE::INVALID_ID);
    read_mem(memvpe, m3::DTU::gaddr_to_virt(pt), buffer, PAGE_SIZE);

    // free all PTEs, if they point to page tables
    size_t ptsize = (1UL << (m3::DTU::LEVEL_BITS * level)) * PAGE_SIZE;
    m3::DTU::pte_t *ptes = reinterpret_cast<m3::DTU::pte_t*>(buffer);
    for(size_t i = 0; i < 1 << m3::DTU::LEVEL_BITS; ++i) {
        if(ptes[i]) {
            /* skip recursive entry */
            if(level == m3::DTU::LEVEL_CNT - 1 && i == m3::DTU::PTE_REC_IDX) {
                virt += ptsize;
                continue;
            }

            gaddr_t gaddr = ptes[i] & ~PAGE_MASK;
            if(level > 1) {
                remove_pts_rec(vpe, gaddr, virt, level - 1);

                // reload the rest of the buffer
                size_t off = i * sizeof(*ptes);
                read_mem(memvpe, m3::DTU::gaddr_to_virt(pt + off), buffer + off, PAGE_SIZE - off);
            }
            // free page table
            KLOG(PTES, "VPE" << vpe << ": lvl " << level << " PTE for " << m3::fmt(virt, "p") << " removed");
            MainMemory::get().free(MainMemory::get().build_allocation(gaddr, PAGE_SIZE));
        }

        virt += ptsize;
    }
}

void DTU::remove_pts(vpeid_t vpe) {
    VPE &v = VPEManager::get().vpe(vpe);
    assert(v.state() == VPE::DEAD);

    // don't destroy page tables of idle VPEs. we need them to execute something on the other PEs
    if(!v.is_idle()) {
        gaddr_t root = v.address_space()->root_pt();
        remove_pts_rec(vpe, root, 0, m3::DTU::LEVEL_CNT - 1);
    }
}

m3::Errors::Code DTU::inval_ep_remote(const kernel::VPEDesc &vpe, epid_t ep) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t reg =
        static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INV_EP) | (ep << 3);
    m3::CPU::compiler_barrier();
    uintptr_t addr = m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::EXT_CMD);
    return try_write_mem(vpe, addr, &reg, sizeof(reg));
}

void DTU::read_ep_remote(const VPEDesc &vpe, epid_t ep, void *regs) {
    m3::CPU::compiler_barrier();
    read_mem(vpe, m3::DTU::ep_regs_addr(ep), regs, sizeof(m3::DTU::reg_t) * m3::DTU::EP_REGS);
}

void DTU::write_ep_remote(const VPEDesc &vpe, epid_t ep, void *regs) {
    m3::CPU::compiler_barrier();
    write_mem(vpe, m3::DTU::ep_regs_addr(ep), regs, sizeof(m3::DTU::reg_t) * m3::DTU::EP_REGS);
}

void DTU::write_ep_local(epid_t ep) {
    m3::DTU::reg_t *src = reinterpret_cast<m3::DTU::reg_t*>(_state.get_ep(ep));
    m3::DTU::reg_t *dst = reinterpret_cast<m3::DTU::reg_t*>(m3::DTU::ep_regs_addr(ep));
    for(size_t i = 0; i < m3::DTU::EP_REGS; ++i)
        dst[i] = src[i];
}

void DTU::mark_read_remote(const VPEDesc &vpe, epid_t ep, uintptr_t msg) {
    m3::DTU::reg_t cmd = static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::ACK_MSG);
    do_ext_cmd(vpe, cmd | (ep << 3) | (static_cast<m3::DTU::reg_t>(msg) << 11));
}

void DTU::drop_msgs(epid_t ep, label_t label) {
    m3::DTU::reg_t *regs = reinterpret_cast<m3::DTU::reg_t*>(_state.get_ep(ep));
    // we assume that the one that used the label can no longer send messages. thus, if there are
    // no messages yet, we are done.
    if((regs[0] & 0xFFFF) == 0)
        return;

    uintptr_t base = regs[1];
    size_t bufsize = (regs[0] >> 16) & 0xFFFF;
    size_t msgsize = (regs[0] >> 32) & 0xFFFF;
    word_t unread = regs[2] >> 32;
    for(size_t i = 0; i < bufsize; ++i) {
        if(unread & (1UL << i)) {
            m3::DTU::Message *msg = reinterpret_cast<m3::DTU::Message*>(base + (i * msgsize));
            if(msg->label == label)
                m3::DTU::get().mark_read(ep, reinterpret_cast<uintptr_t>(msg));
        }
    }
}

static uintptr_t get_msgaddr(const RGateObject *obj, uintptr_t msgaddr) {
    // the message has to be within the receive buffer
    if(!(msgaddr >= obj->addr && msgaddr < obj->addr + obj->size()))
        return 0;

    // ensure that we start at a message boundary
    size_t idx = (msgaddr - obj->addr) >> obj->msgorder;
    return obj->addr + (idx << obj->msgorder);
}

m3::Errors::Code DTU::get_header(const VPEDesc &vpe, const RGateObject *obj, uintptr_t &msgaddr,
        m3::DTU::Header &head) {
    msgaddr = get_msgaddr(obj, msgaddr);
    if(!msgaddr)
        return m3::Errors::INV_ARGS;

    read_mem(vpe, msgaddr, &head, sizeof(head));
    return m3::Errors::NONE;
}

m3::Errors::Code DTU::set_header(const VPEDesc &vpe, const RGateObject *obj, uintptr_t &msgaddr,
        const m3::DTU::Header &head) {
    msgaddr = get_msgaddr(obj, msgaddr);
    if(!msgaddr)
        return m3::Errors::INV_ARGS;

    write_mem(vpe, msgaddr, &head, sizeof(head));
    return m3::Errors::NONE;
}

void DTU::recv_msgs(epid_t ep, uintptr_t buf, int order, int msgorder) {
    _state.config_recv(ep, buf, order, msgorder);
    write_ep_local(ep);
}

void DTU::send_to(const VPEDesc &vpe, epid_t ep, label_t label, const void *msg, size_t size,
        label_t replylbl, epid_t replyep, uint64_t sender) {
    size_t msgsize = size + m3::DTU::HEADER_SIZE;
    _state.config_send(_ep, label, vpe.pe, vpe.id, ep, msgsize, m3::DTU::CREDITS_UNLIM);
    write_ep_local(_ep);

    m3::DTU::get().write_reg(m3::DTU::CmdRegs::DATA, reinterpret_cast<uintptr_t>(msg) | (size << 48));
    m3::DTU::get().write_reg(m3::DTU::CmdRegs::REPLY_LABEL, replylbl);
    if(sender == static_cast<uint64_t>(-1)) {
        sender = Platform::kernel_pe() |
                 (VPEManager::MAX_VPES << 8) |
                 (EP_COUNT << 24) |
                 (static_cast<uint64_t>(replyep) << 32);
    }
    m3::DTU::get().write_reg(m3::DTU::CmdRegs::OFFSET, sender);
    m3::CPU::compiler_barrier();
    m3::DTU::reg_t cmd = m3::DTU::get().buildCommand(_ep, m3::DTU::CmdOpCode::SEND);
    m3::DTU::get().write_reg(m3::DTU::CmdRegs::COMMAND, cmd);

    m3::Errors::Code res = m3::DTU::get().get_error();
    if(res != m3::Errors::NONE)
        PANIC("Send failed");
}

void DTU::reply(epid_t ep, const void *msg, size_t size, size_t msgidx) {
    m3::Errors::Code res = m3::DTU::get().reply(ep, msg, size, msgidx);
    if(res == m3::Errors::VPE_GONE) {
        m3::DTU::Message *rmsg = reinterpret_cast<m3::DTU::Message*>(msgidx);
        // senderVpeId can't be invalid
        VPE &v = VPEManager::get().vpe(rmsg->senderVpeId);
        // the VPE might have been migrated
        rmsg->senderPe = v.pe();
        // re-enable replies
        rmsg->flags |= 1 << 2;
        res = m3::DTU::get().reply(ep, msg, size, msgidx);
    }
    if(res != m3::Errors::NONE)
        PANIC("Reply failed");
}

void DTU::reply_to(const VPEDesc &vpe, epid_t rep, label_t label, const void *msg, size_t size,
        uint64_t sender) {
    send_to(vpe, rep, label, msg, size, 0, 0, sender);
}

m3::Errors::Code DTU::try_write_mem(const VPEDesc &vpe, uintptr_t addr, const void *data, size_t size) {
    if(_state.config_mem_cached(_ep, vpe.pe, vpe.id))
        write_ep_local(_ep);

    // the kernel can never cause pagefaults with reads/writes
    return m3::DTU::get().write(_ep, data, size, addr, m3::DTU::CmdFlags::NOPF);
}

m3::Errors::Code DTU::try_read_mem(const VPEDesc &vpe, uintptr_t addr, void *data, size_t size) {
    if(_state.config_mem_cached(_ep, vpe.pe, vpe.id))
        write_ep_local(_ep);

    return m3::DTU::get().read(_ep, data, size, addr, m3::DTU::CmdFlags::NOPF);
}

void DTU::copy_clear(const VPEDesc &dstvpe, uintptr_t dstaddr,
                     const VPEDesc &srcvpe, uintptr_t srcaddr,
                     size_t size, bool clear) {
    if(clear)
        memset(buffer, 0, sizeof(buffer));

    size_t rem = size;
    while(rem > 0) {
        size_t amount = m3::Math::min(rem, sizeof(buffer));
        // read it from src, if necessary
        if(!clear)
            DTU::get().read_mem(srcvpe, srcaddr, buffer, amount);
        DTU::get().write_mem(dstvpe, dstaddr, buffer, amount);
        srcaddr += amount;
        dstaddr += amount;
        rem -= amount;
    }
}

void DTU::write_swstate(const VPEDesc &vpe, uint64_t flags, uint64_t notify) {
    uint64_t vals[2] = {notify, flags};
    write_mem(vpe, RCTMUX_YIELD, &vals, sizeof(vals));
}

void DTU::write_swflags(const VPEDesc &vpe, uint64_t flags) {
    write_mem(vpe, RCTMUX_FLAGS, &flags, sizeof(flags));
}

void DTU::read_swflags(const VPEDesc &vpe, uint64_t *flags) {
    read_mem(vpe, RCTMUX_FLAGS, flags, sizeof(*flags));
}

}
