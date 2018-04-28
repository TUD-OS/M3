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

#include <base/log/Kernel.h>
#include <base/util/Math.h>

#include "mem/AddrSpace.h"
#include "pes/VPEManager.h"
#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

static const AddrSpace::mmu_pte_t X86_PTE_PRESENT  = 0x1;
static const AddrSpace::mmu_pte_t X86_PTE_WRITE    = 0x2;
static const AddrSpace::mmu_pte_t X86_PTE_USER     = 0x4;
static const AddrSpace::mmu_pte_t X86_PTE_UNCACHED = 0x10;
static const AddrSpace::mmu_pte_t X86_PTE_LARGE    = 0x80;
static const AddrSpace::mmu_pte_t X86_PTE_NOEXEC   = 1ULL << 63;

static char buffer[4096];

AddrSpace::mmu_pte_t AddrSpace::to_mmu_pte(m3::DTU::pte_t pte) {
    if(_pe.has_dtuvm())
        return pte;

#if defined(__x86_64__)
    // the current implementation is based on some equal properties of MMU and DTU paging
    static_assert(sizeof(mmu_pte_t) == sizeof(m3::DTU::pte_t), "MMU and DTU PTEs incompatible");
    static_assert(m3::DTU::LEVEL_CNT == 4, "MMU and DTU PTEs incompatible: levels != 4");
    static_assert(PAGE_SIZE == 4096, "MMU and DTU PTEs incompatible: pagesize != 4k");
    static_assert(m3::DTU::LEVEL_BITS == 9, "MMU and DTU PTEs incompatible: level bits != 9");

    mmu_pte_t res = pte & ~static_cast<m3::DTU::pte_t>(PAGE_MASK);
    // translate NoC address to physical address
    res = (res & ~0xFF00000000000000ULL) | ((res & 0xFF00000000000000ULL) >> 16);

    if(pte & m3::DTU::PTE_RWX)
        res |= X86_PTE_PRESENT;
    if(pte & m3::DTU::PTE_W)
        res |= X86_PTE_WRITE;
    if(pte & m3::DTU::PTE_I)
        res |= X86_PTE_USER;
    if(pte & m3::DTU::PTE_UNCACHED)
        res |= X86_PTE_UNCACHED;
    if(pte & m3::DTU::PTE_LARGE)
        res |= X86_PTE_LARGE;
    if(~pte & m3::DTU::PTE_X)
        res |= X86_PTE_NOEXEC;
    return res;
#else
    return 0;
#endif
}

m3::DTU::pte_t AddrSpace::to_dtu_pte(mmu_pte_t pte) {
    if(_pe.has_dtuvm() || pte == 0)
        return pte;

#if defined(__x86_64__)
    m3::DTU::pte_t res = pte & ~static_cast<m3::DTU::pte_t>(PAGE_MASK);
    // translate physical address to NoC address
    res = (res & ~0x0000FF0000000000ULL) | ((res & 0x0000FF0000000000ULL) << 16);

    if(pte & X86_PTE_PRESENT)
        res |= m3::DTU::PTE_R;
    if(pte & X86_PTE_WRITE)
        res |= m3::DTU::PTE_W;
    if(pte & X86_PTE_USER)
        res |= m3::DTU::PTE_I;
    if(pte & X86_PTE_LARGE)
        res |= m3::DTU::PTE_LARGE;
    if(~pte & X86_PTE_NOEXEC)
        res |= m3::DTU::PTE_X;
    return res;
#else
    return 0;
#endif
}

void AddrSpace::set_rootpt_remote(const VPEDesc &vpe) {
    assert(_pe.has_mmu());
    mmu_cmd_remote(vpe, to_mmu_pte(_root) | m3::DTU::ExtReqOpCode::SET_ROOTPT);
}

void AddrSpace::mmu_cmd_remote(const VPEDesc &vpe, m3::DTU::reg_t arg) {
    assert(arg != 0);
    DTU::get().ext_request(vpe, arg);

    // wait until the remote core sends us an ACK (writes 0 to MASTER_REQ)
    m3::DTU::reg_t mstreq = 1;
    goff_t extarg_addr = m3::DTU::dtu_reg_addr(m3::DTU::ReqRegs::EXT_REQ);
    while(mstreq != 0)
        DTU::get().read_mem(vpe, extarg_addr, &mstreq, sizeof(mstreq));
}

void AddrSpace::setup(const VPEDesc &vpe) {
    // insert recursive entry
    goff_t addr = m3::DTU::gaddr_to_virt(_root);
    m3::DTU::pte_t pte = to_mmu_pte(_root | m3::DTU::PTE_RWX);
    DTU::get().write_mem(VPEDesc(m3::DTU::gaddr_to_pe(_root), VPE::INVALID_ID),
        addr + m3::DTU::PTE_REC_IDX * sizeof(pte), &pte, sizeof(pte));

    if(_pe.has_dtuvm()) {
        static_assert(static_cast<int>(m3::DTU::DtuRegs::FEATURES) == 0, "FEATURES wrong");
        static_assert(static_cast<int>(m3::DTU::DtuRegs::ROOT_PT) == 1, "ROOT_PT wrong");
        static_assert(static_cast<int>(m3::DTU::DtuRegs::PF_EP) == 2, "PF_EP wrong");

        // init DTU registers
        alignas(DTU_PKG_SIZE) m3::DTU::reg_t regs[3];
        uint features = 0;
        if(_sep != static_cast<epid_t>(-1))
            features = static_cast<uint>(m3::DTU::StatusFlags::PAGEFAULTS);
        regs[static_cast<size_t>(m3::DTU::DtuRegs::FEATURES)] = features;
        regs[static_cast<size_t>(m3::DTU::DtuRegs::ROOT_PT)] = _root;
        regs[static_cast<size_t>(m3::DTU::DtuRegs::PF_EP)] = _sep | (_rep << 8);
        m3::CPU::compiler_barrier();
        DTU::get().write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::FEATURES),
            regs, sizeof(regs));
    }
    else {
        alignas(DTU_PKG_SIZE) m3::DTU::reg_t reg = _sep | (_rep << 8);
        m3::CPU::compiler_barrier();
        DTU::get().write_mem(vpe, m3::DTU::dtu_reg_addr(m3::DTU::DtuRegs::PF_EP), &reg, sizeof(reg));

        set_rootpt_remote(vpe);
    }

    // invalidate TLB, because we have changed the root PT
    DTU::get().invtlb_remote(vpe);
}

void AddrSpace::clear_pt(gaddr_t pt) {
    // clear the pagetable
    memset(buffer, 0, sizeof(buffer));
    peid_t pe = m3::DTU::gaddr_to_pe(pt);
    goff_t addr = m3::DTU::gaddr_to_virt(pt);
    for(size_t i = 0; i < PAGE_SIZE / sizeof(buffer); ++i) {
        DTU::get().write_mem(VPEDesc(pe, VPE::INVALID_ID),
            addr + i * sizeof(buffer), buffer, sizeof(buffer));
    }
}

bool AddrSpace::create_pt(const VPEDesc &vpe, goff_t virt, goff_t pteAddr, m3::DTU::pte_t pte,
                          gaddr_t &phys, uint &pages, int perm, int level) {
    // use a large page, if possible
    if(level == 1 && m3::Math::is_aligned(virt, m3::DTU::LPAGE_SIZE) &&
                     pages * PAGE_SIZE >= m3::DTU::LPAGE_SIZE) {
        pte = to_mmu_pte(phys | static_cast<uint>(perm) | m3::DTU::PTE_I | m3::DTU::PTE_LARGE);
        KLOG(PTES, "VPE" << _vpeid << ": lvl " << level << " PTE for "
            << m3::fmt(virt, "p") << ": " << m3::fmt(pte, "#0x", 16));
        DTU::get().write_mem(vpe, pteAddr, &pte, sizeof(pte));
        phys += m3::DTU::LPAGE_SIZE;
        pages -= m3::DTU::LPAGE_SIZE / PAGE_SIZE;
        return true;
    }

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
        pte = to_mmu_pte(pte);
        const size_t ptsize = (1UL << (m3::DTU::LEVEL_BITS * level)) * PAGE_SIZE;
        KLOG(PTES, "VPE" << _vpeid << ": lvl " << level << " PTE for "
            << m3::fmt(virt & ~(ptsize - 1), "p") << ": " << m3::fmt(pte, "#0x", 16));
        DTU::get().write_mem(vpe, pteAddr, &pte, sizeof(pte));
    }
    return false;
}

bool AddrSpace::create_ptes(const VPEDesc &vpe, goff_t &virt, goff_t pteAddr, m3::DTU::pte_t pte,
                            gaddr_t &phys, uint &pages, int perm) {
    // note that we can assume here that map_pages is always called for the same set of
    // pages. i.e., it is not possible that we map page 1 and 2 and afterwards remap
    // only page 1. this is because we call map_pages with MapCapability, which can't
    // be resized. thus, we know that a downgrade for the first, is a downgrade for all
    // and that an existing mapping for the first is an existing mapping for all.

    m3::DTU::pte_t pteDTU = to_dtu_pte(pte);
    m3::DTU::pte_t npte = phys | static_cast<uint>(perm) | m3::DTU::PTE_I;
    if(npte == pteDTU)
        return true;

    bool downgrade = false;
    // do not invalidate pages if we are writing to a memory PE
    if((pteDTU & m3::DTU::PTE_RWX) && Platform::pe(vpe.pe).has_virtmem())
        downgrade = ((pteDTU & m3::DTU::PTE_RWX) & (~npte & m3::DTU::PTE_RWX)) != 0;

    goff_t endpte = m3::Math::min(pteAddr + pages * sizeof(npte),
        m3::Math::round_up(pteAddr + sizeof(npte), static_cast<goff_t>(PAGE_SIZE)));

    uint count = (endpte - pteAddr) / sizeof(npte);
    assert(count > 0);
    pages -= count;
    phys += count << PAGE_BITS;

    npte = to_mmu_pte(npte);
    while(pteAddr < endpte) {
        size_t i = 0;
        goff_t startAddr = pteAddr;
        m3::DTU::pte_t buf[16];
        for(; pteAddr < endpte && i < ARRAY_SIZE(buf); ++i) {
            KLOG(PTES, "VPE" << _vpeid << ": lvl 0 PTE for "
                << m3::fmt(virt, "p") << ": " << m3::fmt(npte, "#0x", 16)
                << (downgrade ? " (invalidating)" : "")
                << (Platform::pe(vpe.pe).type() == m3::PEType::MEM ? " (to mem)" : ""));

            buf[i] = npte;

            pteAddr += sizeof(npte);
            virt += PAGE_SIZE;
            npte += PAGE_SIZE;
        }

        DTU::get().write_mem(vpe, startAddr, buf, i * sizeof(buf[0]));

        if(downgrade) {
            for(goff_t vaddr = virt - i * PAGE_SIZE; vaddr < virt; vaddr += PAGE_SIZE) {
                if(_pe.has_mmu())
                    mmu_cmd_remote(vpe, vaddr | m3::DTU::ExtReqOpCode::INV_PAGE);
                DTU::get().invlpg_remote(vpe, vaddr);
            }
        }
    }
    return false;
}

static goff_t get_pte_addr(goff_t virt, int level) {
    static goff_t recMask =
        (static_cast<goff_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 3)) |
        (static_cast<goff_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 2)) |
        (static_cast<goff_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 1)) |
        (static_cast<goff_t>(m3::DTU::PTE_REC_IDX) << (PAGE_BITS + m3::DTU::LEVEL_BITS * 0));

    // at first, just shift it accordingly.
    virt >>= PAGE_BITS + level * m3::DTU::LEVEL_BITS;
    virt <<= m3::DTU::PTE_BITS;

    // now put in one PTE_REC_IDX's for each loop that we need to take
    int mask_shift = (m3::DTU::LEVEL_BITS * (m3::DTU::LEVEL_CNT - (level + 1)) + PAGE_BITS);
    goff_t remMask = (static_cast<goff_t>(1) << mask_shift) - 1;
    virt |= recMask & ~remMask;

    // finally, make sure that we stay within the bounds for virtual addresses
    // this is because of recMask, that might actually have too many of those.
    virt &= (static_cast<goff_t>(1) << (m3::DTU::LEVEL_CNT * m3::DTU::LEVEL_BITS + PAGE_BITS)) - 1;
    return virt;
}

goff_t AddrSpace::get_pte_addr_mem(const VPEDesc &vpe, gaddr_t root, goff_t virt, int level) {
    goff_t pt = m3::DTU::gaddr_to_virt(root);
    for(int l = m3::DTU::LEVEL_CNT - 1; l >= 0; --l) {
        size_t idx = (virt >> (PAGE_BITS + m3::DTU::LEVEL_BITS * l)) & m3::DTU::LEVEL_MASK;
        pt += idx * m3::DTU::PTE_SIZE;

        if(level == l)
            return pt;

        m3::DTU::pte_t pte;
        DTU::get().read_mem(vpe, pt, &pte, sizeof(pte));
        pte = to_dtu_pte(pte);

        pt = m3::DTU::gaddr_to_virt(pte & ~PAGE_MASK);
    }

    UNREACHED;
}

void AddrSpace::map_pages(const VPEDesc &vpe, goff_t virt, gaddr_t phys, uint pages, int perm) {
    bool running = vpe.pe == Platform::kernel_pe() || VPEManager::get().vpe(vpe.id).is_on_pe();

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
            goff_t pteAddr;
            if(!running)
                pteAddr = get_pte_addr_mem(rvpe, root, virt, level);
            else
                pteAddr = get_pte_addr(virt, level);

            m3::DTU::pte_t pte;
            DTU::get().read_mem(rvpe, pteAddr, &pte, sizeof(pte));
            pte = to_dtu_pte(pte);

            if(level > 0) {
                if(create_pt(rvpe, virt, pteAddr, pte, phys, pages, perm, level))
                    break;
            }
            else {
                if(create_ptes(rvpe, virt, pteAddr, pte, phys, pages, perm))
                    return;
            }
        }
    }
}

void AddrSpace::unmap_pages(const VPEDesc &vpe, goff_t virt, uint pages) {
    // don't do anything if the VPE is already dead
    if(vpe.pe != Platform::kernel_pe() && VPEManager::get().vpe(vpe.id).state() == VPE::DEAD)
        return;

    map_pages(vpe, virt, 0, pages, 0);
}

void AddrSpace::remove_pts_rec(const VPEDesc &vpe, gaddr_t pt, goff_t virt, int level) {
    static_assert(sizeof(buffer) >= PAGE_SIZE, "Buffer smaller than a page");

    // load entire page table
    peid_t pe = m3::DTU::gaddr_to_pe(pt);
    VPEDesc memvpe(pe, VPE::INVALID_ID);
    DTU::get().read_mem(memvpe, m3::DTU::gaddr_to_virt(pt), buffer, PAGE_SIZE);

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

            gaddr_t gaddr = to_dtu_pte(ptes[i]) & ~static_cast<gaddr_t>(PAGE_MASK);
            if(level > 1) {
                remove_pts_rec(vpe, gaddr, virt, level - 1);

                // reload the rest of the buffer
                size_t off = i * sizeof(*ptes);
                DTU::get().read_mem(memvpe, m3::DTU::gaddr_to_virt(pt + off), buffer + off, PAGE_SIZE - off);
            }
            // free page table
            KLOG(PTES, "VPE" << vpe.id << ": lvl " << level << " PTE for " << m3::fmt(virt, "p") << " removed");
            MainMemory::get().free(MainMemory::get().build_allocation(gaddr, PAGE_SIZE));
        }

        virt += ptsize;
    }
}

void AddrSpace::remove_pts(vpeid_t vpe) {
    VPE &v = VPEManager::get().vpe(vpe);
    assert(v.state() == VPE::DEAD);

    // don't destroy page tables of idle VPEs. we need them to execute something on the other PEs
    if(!v.is_idle()) {
        gaddr_t root = v.address_space()->root_pt();
        remove_pts_rec(v.desc(), root, 0, m3::DTU::LEVEL_CNT - 1);
    }
}

}
