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

#pragma once

#include <base/Common.h>
#include <base/DTU.h>

#include "mem/MainMemory.h"
#include "pes/VPEDesc.h"
#include "Platform.h"

namespace kernel {

class AddrSpace {
public:
    typedef uint64_t mmu_pte_t;

    // for the kernel
    explicit AddrSpace(vpeid_t vpeid)
        : _pe(Platform::pe(Platform::kernel_pe())),
          _vpeid(vpeid),
          _sep(),
          _rep(),
          _sgate(),
          _root() {
    }

    explicit AddrSpace(peid_t pe, vpeid_t vpeid, epid_t sep, epid_t rep, capsel_t sgate);
    ~AddrSpace();

    epid_t sep() const {
        return _sep;
    }
    epid_t rep() const {
        return _rep;
    }
    capsel_t sgate() const {
        return _sgate;
    }

    gaddr_t root_pt() const {
        return _root;
    }

    void setup(const VPEDesc &vpe);

    void map_pages(const VPEDesc &vpe, goff_t virt, gaddr_t phys, uint pages, int perm);
    void unmap_pages(const VPEDesc &vpe, goff_t virt, uint pages);
    void remove_pts(vpeid_t vpe);

private:
#if defined(__gem5__)
    void clear_pt(gaddr_t pt);
    bool create_pt(const VPEDesc &vpe, goff_t &virt, goff_t pteAddr, m3::DTU::pte_t pte,
                   gaddr_t &phys, uint &pages, int perm, int level);
    bool create_ptes(const VPEDesc &vpe, goff_t &virt, goff_t pteAddr, m3::DTU::pte_t pte,
                     gaddr_t &phys, uint &pages, int perm);

    void remove_pts_rec(const VPEDesc &vpe, gaddr_t pt, goff_t virt, int level);

    goff_t get_pte_addr_mem(const VPEDesc &vpe, gaddr_t root, goff_t virt, int level);

    mmu_pte_t to_mmu_pte(m3::DTU::pte_t pte);
    m3::DTU::pte_t to_dtu_pte(mmu_pte_t pte);

    void mmu_cmd_remote(const VPEDesc &vpe, m3::DTU::reg_t arg);
#endif

    m3::PEDesc _pe;
    vpeid_t _vpeid;
    epid_t _sep;
    epid_t _rep;
    capsel_t _sgate;
    gaddr_t _root;
};

}
