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

#include "mem/MemoryMap.h"

namespace kernel {

class MemoryModule {
public:
    explicit MemoryModule(bool avail, size_t pe, uintptr_t addr, size_t size)
        : _avail(avail), _pe(pe), _addr(addr), _size(size), _map(addr, size) {
    }

    bool available() const {
        return _avail;
    }
    size_t pe() const {
        return _pe;
    }
    uintptr_t addr() const {
        return _addr;
    }
    size_t size() const {
        return _size;
    }
    MemoryMap &map() {
        return _map;
    }

private:
    bool _avail;
    size_t _pe;
    uintptr_t _addr;
    size_t _size;
    MemoryMap _map;
};

}
