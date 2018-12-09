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

#include <base/col/SList.h>

#include "Region.h"

class RegionList {
public:
    typedef m3::SList<Region>::iterator iterator;

    explicit RegionList(DataSpace *ds)
        : _ds(ds),
          _regs() {
    }
    RegionList(const RegionList &) = delete;
    RegionList &operator=(const RegionList &) = delete;
    ~RegionList() {
        clear();
    }

    void clear();

    void append(Region *r) {
        _regs.append(r);
    }

    size_t count() const {
        return _regs.length();
    }
    iterator begin() {
        return _regs.begin();
    }
    iterator end() {
        return _regs.end();
    }

    Region *pagefault(goff_t offset);

    void print(m3::OStream &os) const;

private:
    DataSpace *_ds;
    m3::SList<Region> _regs;
};
