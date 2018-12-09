/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
#include <base/util/Math.h>
#include <base/Env.h>
#include <base/Heap.h>

#include "mem/MainMemory.h"
#include "pes/VPEManager.h"
#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"
#include "WorkLoop.h"

namespace kernel {

class BaremetalKEnvBackend : public m3::BaremetalEnvBackend {
public:
    explicit BaremetalKEnvBackend() {
        _workloop = new WorkLoop();
    }

    virtual void init() override {
        // don't do that on gem5 because the kernel peid is already set by gem5
#if !defined(__gem5__)
        m3::env()->pe = DTU::get().log_to_phys(Platform::kernel_pe());
#endif

        m3::Serial::init("kernel", m3::env()->pe);
    }

    virtual void reinit() override {
        // not used
    }

    virtual bool extend_heap(size_t size) override {
        if(!Platform::pe(Platform::kernel_pe()).has_virtmem())
            return false;
        // TODO currently not supported
        if(Platform::pe(Platform::kernel_pe()).has_mmu())
            return false;

        uint pages = m3::Math::max((size_t)8,
            m3::Math::round_up<size_t>(size, PAGE_SIZE) >> PAGE_BITS);

        // allocate memory
        MainMemory::Allocation alloc = MainMemory::get().allocate(pages * PAGE_SIZE, PAGE_SIZE);
        if(!alloc)
            return false;

        // map the memory
        uintptr_t virt = m3::Math::round_up<uintptr_t>(
            reinterpret_cast<uintptr_t>(heap_end), PAGE_SIZE);
        gaddr_t phys = m3::DTU::build_gaddr(alloc.pe(), alloc.addr);

        VPEDesc vpe(Platform::kernel_pe(), VPEManager::MAX_VPES);
        AddrSpace kas(vpe.id);
        kas.map_pages(vpe, virt, phys, pages, m3::KIF::Perm::RW);

        m3::Heap::append(pages);
        return true;
    }

    virtual void exit(int) override {
    }
};

EXTERN_C void init_env(m3::Env *e) {
    m3::Heap::init();
    e->_backend = reinterpret_cast<uint64_t>(new BaremetalKEnvBackend());
}

}
