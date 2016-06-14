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

#include <base/col/SList.h>

#include "Region.h"

class RegionList {
public:
    typedef m3::SList<Region>::iterator iterator;

    explicit RegionList(uintptr_t virt, size_t total) : _virt(virt), _total(total), _regs() {
    }
    RegionList(const RegionList &) = delete;
    RegionList &operator=(const RegionList &) = delete;
    ~RegionList() {
        while(_regs.length() > 0) {
            Region *r = _regs.remove_first();
            delete r;
        }
    }

    void append(Region *r) {
        _regs.append(r);
    }

    iterator begin() {
        return _regs.begin();
    }
    iterator end() {
        return _regs.end();
    }

    Region *pagefault(uintptr_t offset) {
        Region *last = nullptr;
        auto r = _regs.begin();
        // search for the region that contains <offset> or is behind <offset>
        if(_regs.length() > 0) {
            while(r != _regs.end() && r->offset() + r->size() <= offset) {
                last = &*r;
                r++;
            }
        }

        // does it contain <offset>?
        if(r != _regs.end() && offset >= r->offset() && offset < r->offset() + r->size())
            return &*r;

        // ok, build a new region that spans from the previous one to the next one
        uintptr_t end = r == _regs.end() ? _total : r->offset();
        uintptr_t start = last ? last->offset() + last->size() : 0;
        Region *n = new Region(_virt, start, end - start);
        _regs.insert(last, n);
        return n;
    }

    void print(m3::OStream &os) const {
        for(auto reg = _regs.begin(); reg != _regs.end(); ++reg) {
            os << "    " << m3::fmt(_virt + reg->offset(), "p");
            os << " .. " << m3::fmt(_virt + reg->offset() + reg->size() - 1, "p");
            os << " COW=" << ((reg->flags() & Region::COW) ? "1" : "0");
            os << " -> ";
            reg->mem()->print(os);
            os << "\n";
        }
    }

private:
    uintptr_t _virt;
    size_t _total;
    m3::SList<Region> _regs;
};
