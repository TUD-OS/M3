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
#include <base/util/Reference.h>
#include <base/log/Services.h>
#include <base/DTU.h>

#include <m3/com/MemGate.h>

#include "PhysMem.h"

void copy_block(m3::MemGate *src, m3::MemGate *dst, size_t srcoff, size_t size);

class DataSpace;

/**
 * A region is a part of a dataspace, that allows us to allocate, copy, etc. smaller parts of the
 * dataspace.
 */
class Region : public m3::SListItem {
public:
    enum Flags {
        COW     = 1 << 0,
    };

    explicit Region(DataSpace *ds, size_t offset, size_t size)
        : SListItem(), _mem(), _ds(ds), _offset(offset), _memoff(), _size(size), _flags() {
    }
    Region(const Region &r)
        : SListItem(r), _mem(r._mem), _ds(r._ds), _offset(r._offset), _memoff(r._memoff),
          _size(r._size), _flags(r._flags) {
    }
    Region &operator=(const Region &r) = delete;
    ~Region();

    void ds(DataSpace *ds) {
        _ds = ds;
    }

    bool has_mem() const {
        return _mem.valid();
    }
    PhysMem *mem() {
        return has_mem() ? &*_mem : nullptr;
    }
    const PhysMem *mem() const {
        return has_mem() ? &*_mem : nullptr;
    }
    void mem(PhysMem *mem) {
        _mem = m3::Reference<PhysMem>(mem);
    }

    uint flags() const {
        return _flags;
    }
    void flags(uint flags) {
        _flags = flags;
    }

    uintptr_t virt() const;
    m3::Errors::Code map(uint flags);
    void copy(m3::MemGate *mem, uintptr_t virt);
    void clear();

    size_t offset() const {
        return _offset;
    }
    void offset(size_t offset) {
        // the offset can only be increased, but not decreased
        assert(offset >= _offset);
        assert(_size > offset - _offset);
        _size -= offset - _offset;
        _offset = offset;
    }

    size_t mem_offset() const {
        return _memoff;
    }
    void mem_offset(size_t off) {
        _memoff = off;
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
    m3::Reference<PhysMem> _mem;
    DataSpace *_ds;
    size_t _offset;
    size_t _memoff;
    size_t _size;
    uint _flags;
};
