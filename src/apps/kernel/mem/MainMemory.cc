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

#include <base/Init.h>

#include "mem/MainMemory.h"

namespace kernel {

INIT_PRIO_USER(1) MainMemory MainMemory::_inst;

MainMemory::MainMemory() : _count(), _mods() {
}

void MainMemory::add(MemoryModule *mod) {
    if(_count == MAX_MODS)
        PANIC("No free memory module slots");
    _mods[_count++] = mod;
}

const MemoryModule &MainMemory::module(size_t id) const {
    assert(_mods[id] != nullptr);
    return *_mods[id];
}

MainMemory::Allocation MainMemory::allocate(size_t size, size_t align) {
    for(size_t i = 0; i < _count; ++i) {
        if(!_mods[i]->available())
            continue;
        uintptr_t res = _mods[i]->map().allocate(size, align);
        if(res)
            return Allocation(i, res, size);
    }
    return Allocation();
}

MainMemory::Allocation MainMemory::allocate_at(uintptr_t offset, size_t size) {
    // TODO this is not final
    for(size_t i = 0; i < _count; ++i) {
        if(!_mods[i]->available())
            return Allocation(i, _mods[i]->addr() + offset, size);
    }
    return Allocation();
}

void MainMemory::free(peid_t pe, uintptr_t addr, size_t size) {
    for(size_t i = 0; i < _count; ++i) {
        if(_mods[i]->pe() == pe) {
            _mods[i]->map().free(addr, size);
            break;
        }
    }
}

void MainMemory::free(const Allocation &alloc) {
    assert(_mods[alloc.mod] != nullptr);
    _mods[alloc.mod]->map().free(alloc.addr, alloc.size);
}

size_t MainMemory::size() const {
    size_t total = 0;
    for(size_t i = 0; i < _count; ++i) {
        if(_mods[i]->available())
            total += _mods[i]->size();
    }
    return total;
}

size_t MainMemory::available() const {
    size_t total = 0;
    for(size_t i = 0; i < _count; ++i) {
        if(_mods[i]->available())
            total += _mods[i]->map().get_size();
    }
    return total;
}

m3::OStream &operator<<(m3::OStream &os, const MainMemory &mem) {
    os << "Main Memory[total=" << (mem.size() / 1024) << " KiB,"
       << " free=" << (mem.available() / 1024) << " KiB]:\n";
    for(size_t i = 0; i < mem._count; ++i) {
        os << "  " << (mem._mods[i]->available() ? "free" : "used");
        os << " pe=" << mem._mods[i]->pe() << " addr=" << m3::fmt(mem._mods[i]->addr(), "p");
        os << " size=" << m3::fmt(mem._mods[i]->size(), "p") << "\n";
    }
    return os;
}

}
