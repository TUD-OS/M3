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
    char buffer[1024];
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

void DTU::config_rwb_remote(const VPEDesc &vpe, uintptr_t addr) {
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t barrier = addr;
    m3::CPU::compiler_barrier();
    write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::RW_BARRIER), &barrier, sizeof(barrier));
}

void DTU::config_pf_remote(const VPEDesc &vpe, gaddr_t rootpt, epid_t ep) {
    static_assert(static_cast<int>(m3::DTU::DtuRegs::FEATURES) == 0, "FEATURES wrong");
    static_assert(static_cast<int>(m3::DTU::DtuRegs::ROOT_PT) == 1, "ROOT_PT wrong");
    static_assert(static_cast<int>(m3::DTU::DtuRegs::PF_EP) == 2, "PF_EP wrong");

    // init root PT
    clear_pt(rootpt);

    // insert recursive entry
    uintptr_t addr = m3::DTU::gaddr_to_virt(rootpt);
    m3::DTU::pte_t pte = rootpt | m3::DTU::PTE_RWX;
    write_mem(VPEDesc(m3::DTU::gaddr_to_pe(rootpt), VPE::INVALID_ID),
        addr + m3::DTU::PTE_REC_IDX * sizeof(pte), &pte, sizeof(pte));

    // init DTU registers
    alignas(DTU_PKG_SIZE) m3::DTU::reg_t regs[3];
    uint features = 0;
    if(ep != static_cast<epid_t>(-1))
        features = static_cast<uint>(m3::DTU::StatusFlags::PAGEFAULTS);
    regs[static_cast<size_t>(m3::DTU::DtuRegs::FEATURES)] = features;
    regs[static_cast<size_t>(m3::DTU::DtuRegs::ROOT_PT)] = rootpt;
    regs[static_cast<size_t>(m3::DTU::DtuRegs::PF_EP)] = ep;
    m3::CPU::compiler_barrier();
    write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::FEATURES), regs, sizeof(regs));

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
        MainMemory::Allocation alloc = MainMemory::get().allocate(PAGE_SIZE, PAGE_SIZE);
        assert(alloc);

        // clear PT
        pte = m3::DTU::build_gaddr(alloc.pe(), alloc.addr);
        clear_pt(pte);

        // insert PTE
        pte |= m3::DTU::PTE_RWX;
        KLOG(PTES, "PE" << vpe.pe << ": lvl 1 PTE for "
            << m3::fmt(virt, "p") << ": " << m3::fmt(pte, "#0x", 16));
        write_mem(vpe, pteAddr, &pte, sizeof(pte));
    }

    assert((pte & m3::DTU::PTE_IRWX) == m3::DTU::PTE_RWX);
    return false;
}

bool DTU::create_ptes(const VPEDesc &vpe, uintptr_t &virt, uintptr_t pteAddr, m3::DTU::pte_t pte,
        gaddr_t &phys, uint &pages, int perm) {
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
    // do not invalidate pages if we are writing to a memory PE
    downgrade &= Platform::pe(vpe.pe).has_virtmem();
    uintptr_t endpte = m3::Math::min(pteAddr + pages * sizeof(npte),
        m3::Math::round_up(pteAddr + sizeof(npte), PAGE_SIZE));

    uint count = (endpte - pteAddr) / sizeof(npte);
    assert(count > 0);
    pages -= count;
    phys += count << PAGE_BITS;

    while(pteAddr < endpte) {
        KLOG(PTES, "PE" << vpe.pe << ": lvl 0 PTE for "
            << m3::fmt(virt, "p") << ": " << m3::fmt(npte, "#0x", 16));
        write_mem(vpe, pteAddr, &npte, sizeof(npte));

        // permissions downgraded?
        if(downgrade) {
            // do that manually instead of with do_ext_cmd, because we don't want to reconfigure
            // the endpoint
            alignas(DTU_PKG_SIZE) m3::DTU::reg_t reg =
                static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::INV_PAGE) | (virt << 3);
            m3::CPU::compiler_barrier();
            write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::EXT_CMD), &reg, sizeof(reg));
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
                if(create_pt(rvpe, virt, pteAddr, pte, perm))
                    return;
            }
            else {
                if(create_ptes(rvpe, virt, pteAddr, pte, phys, pages, perm))
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

    // TODO remove pagetables on demand
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

void DTU::recv_msgs(epid_t ep, uintptr_t buf, uint order, uint msgorder) {
    _state.config_recv(ep, buf, order, msgorder);
    write_ep_local(ep);
}

void DTU::send_to(const VPEDesc &vpe, epid_t ep, label_t label, const void *msg, size_t size,
        label_t replylbl, epid_t replyep, uint64_t sender) {
    size_t msgsize = size + m3::DTU::HEADER_SIZE;
    _state.config_send(_ep, label, vpe.pe, vpe.id, ep, msgsize, m3::DTU::CREDITS_UNLIM);
    write_ep_local(_ep);

    m3::DTU::get().write_reg(m3::DTU::CmdRegs::DATA_ADDR, reinterpret_cast<uintptr_t>(msg));
    m3::DTU::get().write_reg(m3::DTU::CmdRegs::DATA_SIZE, size);
    m3::DTU::get().write_reg(m3::DTU::CmdRegs::REPLY_LABEL, replylbl);
    if(sender == static_cast<uint64_t>(-1)) {
        sender = Platform::kernel_pe() |
                 (VPEManager::MAX_VPES << 8) |
                 (_ep << 24) |
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
    _state.config_mem(_ep, vpe.pe, vpe.id, addr, size, m3::DTU::W);
    write_ep_local(_ep);

    // the kernel can never cause pagefaults with reads/writes
    return m3::DTU::get().write(_ep, data, size, 0, m3::DTU::CmdFlags::NOPF);
}

m3::Errors::Code DTU::try_read_mem(const VPEDesc &vpe, uintptr_t addr, void *data, size_t size) {
    _state.config_mem(_ep, vpe.pe, vpe.id, addr, size, m3::DTU::R);
    write_ep_local(_ep);

    return m3::DTU::get().read(_ep, data, size, 0, m3::DTU::CmdFlags::NOPF);
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
