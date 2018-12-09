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

#include "DataSpace.h"
#include "RegionList.h"

void RegionList::clear() {
    while(_regs.length() > 0) {
        Region *r = _regs.remove_first();
        delete r;
    }
}

Region *RegionList::pagefault(goff_t offset) {
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
    goff_t end = r == _regs.end() ? _ds->size() : r->offset();
    goff_t start = last ? last->offset() + last->size() : 0;
    Region *n = new Region(_ds, start, end - start);
    _regs.insert(last, n);
    return n;
}

void RegionList::print(m3::OStream &os) const {
    for(auto reg = _regs.begin(); reg != _regs.end(); ++reg) {
        os << "    " << m3::fmt(_ds->addr() + reg->offset(), "p");
        os << " .. " << m3::fmt(_ds->addr() + reg->offset() + reg->size() - 1, "p");
        os << " COW=" << ((reg->flags() & Region::COW) ? "1" : "0");
        os << " -> ";
        reg->mem()->print(os);
        os << "\n";
    }
}
