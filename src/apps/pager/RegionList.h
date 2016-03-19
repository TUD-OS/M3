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
#include <base/col/SList.h>
#include <base/util/CapRngDesc.h>
#include <base/DTU.h>

#include <m3/com/MemGate.h>

namespace m3 {

class Region : public SListItem {
public:
    explicit Region(uintptr_t offset, size_t size)
        : SListItem(), _mem(), _offset(offset), _size(size) {
    }
    ~Region() {
        delete _mem;
    }

    MemGate *mem() {
        return _mem;
    }
    const MemGate *mem() const {
        return _mem;
    }
    void mem(MemGate *gate) {
        _mem = gate;
    }

    uintptr_t offset() const {
        return _offset;
    }
    void offset(uintptr_t offset) {
        // the size can only be increased, but not decreased
        assert(offset >= _offset);
        assert(_size > offset - _offset);
        _size -= offset - _offset;
        _offset = offset;
    }

    size_t size() const {
        return _size;
    }
    void size(size_t size) {
        // the size can only be decreased, but not increased
        assert(size <= _size);
        _size = size;
    }

private:
    MemGate *_mem;
    uintptr_t _offset;
    size_t _size;
};

class RegionList {
public:
    explicit RegionList(size_t total) : _total(total), _regs() {
    }
    ~RegionList() {
        while(_regs.length() > 0) {
            Region *r = _regs.remove_first();
            delete r;
        }
    }

    Region *pagefault(uintptr_t offset) {
        uintptr_t last = 0;
        auto r = _regs.begin();
        // search for the region that contains <offset> or is behind <offset>
        if(_regs.length() > 0) {
            while(r->offset() + r->size() <= offset) {
                last = r->offset() + r->size();
                r++;
            }
        }

        // does it contain <offset>?
        if(r != _regs.end() && offset >= r->offset() && offset < r->offset() + r->size())
            return &*r;

        // ok, build a new region that spans from the previous one to the next one
        uintptr_t end = r == _regs.end() ? _total : r->offset();
        Region *n = new Region(last, end - last);
        _regs.append(n);
        return n;
    }

private:
    size_t _total;
    SList<Region> _regs;
};

}
