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

#include "mem/MemoryModule.h"

namespace m3 {
    class OStream;
}

namespace kernel {

class MainMemory {
    explicit MainMemory();

    static const size_t MAX_MODS    = 4;

public:
    struct Allocation {
        explicit Allocation()
            : mod(),
              addr(),
              size() {
        }
        explicit Allocation(size_t _mod, goff_t _addr, size_t _size)
            : mod(_mod),
              addr(_addr),
              size(_size) {
        }

        operator bool() const {
            return size > 0;
        }
        peid_t pe() const {
            return MainMemory::get().module(mod).pe();
        }

        size_t mod;
        goff_t addr;
        size_t size;
    };

    static MainMemory &get() {
        return _inst;
    }

    void add(MemoryModule *mod);

    const MemoryModule &module(size_t id) const;
    Allocation build_allocation(gaddr_t addr, size_t size) const;

    Allocation allocate(size_t size, size_t align);
    Allocation allocate_at(goff_t offset, size_t size);

    void free(peid_t pe, goff_t addr, size_t size);
    void free(const Allocation &alloc);

    size_t size() const;
    size_t available() const;

    friend m3::OStream &operator<<(m3::OStream &os, const MainMemory &mem);

private:
    size_t _count;
    MemoryModule *_mods[MAX_MODS];
    static MainMemory _inst;
};

}
