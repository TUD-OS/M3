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
#include <base/stream/Serial.h>
#include <base/Backtrace.h>
#include <base/Env.h>
#include <base/Heap.h>

#include "mem/MainMemory.h"
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
        // don't do that on gem5 because the kernel coreid is already set by gem5
#if !defined(__gem5__)
        m3::env()->coreid = DTU::get().log_to_phys(Platform::kernel_pe());
#endif

        m3::Serial::init("kernel", m3::env()->coreid);
    }

    virtual void reinit() override {
        // not used
    }

    virtual bool extend_heap(size_t size) override {
        if(!Platform::pe(Platform::kernel_pe()).has_virtmem())
            return false;

        uint pages = m3::Math::max((size_t)8,
            m3::Math::round_up<size_t>(size, PAGE_SIZE) >> PAGE_BITS);

        // allocate memory
        MainMemory::Allocation alloc = MainMemory::get().allocate(pages * PAGE_SIZE, PAGE_SIZE);
        if(!alloc)
            return false;

        // map the memory
        uintptr_t virt = m3::Math::round_up<uintptr_t>(
            reinterpret_cast<uintptr_t>(m3::Heap::_end), PAGE_SIZE);
        uint64_t phys = m3::DTU::build_noc_addr(alloc.pe(), alloc.addr);
        VPEDesc vpe(Platform::kernel_pe(), Platform::kernel_pe());
        DTU::get().map_pages(vpe, virt, phys, pages, m3::KIF::Perm::RW);

        // build new end Area and connect it
        m3::Heap::Area *end = reinterpret_cast<m3::Heap::Area*>(virt + pages * PAGE_SIZE) - 1;
        end->next = 0;
        m3::Heap::Area *prev = m3::Heap::backwards(m3::Heap::_end, m3::Heap::_end->prev);
        // if the last area is used, we have to keep _end and put us behind there
        if(m3::Heap::is_used(prev)) {
            end->prev = (end - m3::Heap::_end) * sizeof(m3::Heap::Area);
            m3::Heap::_end->next = end->prev;
        }
        // otherwise, merge it into the last area
        else {
            end->prev = m3::Heap::_end->prev + pages * PAGE_SIZE;
            prev->next += pages * PAGE_SIZE;
        }
        m3::Heap::_end = end;
        return true;
    }

    virtual void exit(int) override {
    }
};

EXTERN_C void init_env(m3::Env *e) {
    e->backend = new BaremetalKEnvBackend();
}

}
