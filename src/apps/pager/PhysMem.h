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

#pragma once

#include <base/stream/OStream.h>
#include <base/util/Reference.h>

#include <m3/com/MemGate.h>

class Region;

/**
 * Physical memory that might be shared among multiple address spaces. There might be an owner of
 * the memory, which is the one that might have dirty data it his cache. If there is, we want to
 * copy from there instead of from the actual memory.
 */
class PhysMem : public m3::RefCounted {
    friend class Region;

    explicit PhysMem(m3::MemGate *mem, m3::MemGate *gate, goff_t virt)
        : RefCounted(),
          gate(gate),
          owner_mem(mem),
          owner_virt(virt) {
    }

public:
    explicit PhysMem(m3::MemGate *mem, goff_t virt, size_t size, int perm)
        : RefCounted(),
          gate(new m3::MemGate(m3::MemGate::create_global(size, perm))),
          owner_mem(mem),
          owner_virt(virt) {
    }
    explicit PhysMem(m3::MemGate *mem, goff_t virt, capsel_t sel)
        : RefCounted(),
          gate(new m3::MemGate(m3::MemGate::bind(sel))),
          owner_mem(mem),
          owner_virt(virt) {
    }
    ~PhysMem() {
        delete gate;
    }

    bool is_last() const {
        return refcount() == 1;
    }

    void print(m3::OStream &os) const {
        os << "id: " << m3::fmt(gate->sel(), 3) << " refs: " << refcount();
        os << " [owner=" << owner_mem << " @ " << m3::fmt(owner_virt, "p") << "]";
    }

    m3::MemGate *gate;
    m3::MemGate *owner_mem;
    goff_t owner_virt;
};
