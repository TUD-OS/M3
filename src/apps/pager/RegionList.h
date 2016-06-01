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

namespace m3 {

static char zeros[4096];
static char tmpbuf[4096];

static void copy_block(MemGate *src, MemGate *dst, size_t srcoff, size_t size) {
    size_t pages = size >> PAGE_BITS;
    for(size_t i = 0; i < pages; ++i) {
        src->read_sync (tmpbuf, sizeof(tmpbuf), srcoff + i * PAGE_SIZE);
        dst->write_sync(tmpbuf, sizeof(tmpbuf), i * PAGE_SIZE);
    }
}

class Region;

/**
 * Physical memory that might be shared among multiple address spaces. There might be an owner of
 * the memory, which is the one that might have dirty data it his cache. If there is, we want to
 * copy from there instead of from the actual memory.
 */
class PhysMem : public RefCounted {
    friend class Region;

    explicit PhysMem(MemGate *mem, MemGate *gate, uintptr_t virt)
        : RefCounted(), gate(gate), owner_mem(mem), owner_virt(virt) {
    }

public:
    explicit PhysMem(MemGate *mem, uintptr_t virt, size_t size, uint perm)
        : RefCounted(), gate(new MemGate(MemGate::create_global(size, perm))),
          owner_mem(mem), owner_virt(virt) {
    }
    explicit PhysMem(MemGate *mem, uintptr_t virt, capsel_t sel)
        : RefCounted(), gate(new MemGate(MemGate::bind(sel))),
          owner_mem(mem), owner_virt(virt) {
    }
    ~PhysMem() {
        delete gate;
    }

    bool is_last() const {
        return refcount() == 1;
    }

    void print(OStream &os) const {
        os << "id: " << fmt(gate->sel(), 3) << " refs: " << refcount();
        os << " [owner=" << owner_mem << " @ " << fmt(owner_virt, "p") << "]";
    }

    MemGate *gate;
    MemGate *owner_mem;
    uintptr_t owner_virt;
};

/**
 * A region is a part of a dataspace, that allows us to allocate, copy, etc. smaller parts of the
 * dataspace.
 */
class Region : public SListItem {
public:
    enum Flags {
        COW     = 1 << 0,
    };

    explicit Region(uintptr_t offset, size_t size)
        : SListItem(), _mem(), _offset(offset), _memoff(), _size(size), _flags() {
    }
    Region(const Region &r)
        : SListItem(r), _mem(r._mem), _offset(r._offset), _memoff(r._memoff),
          _size(r._size), _flags(r._flags) {
    }
    Region &operator=(const Region &r) = delete;

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
        _mem = Reference<PhysMem>(mem);
    }

    uint flags() const {
        return _flags;
    }
    void flags(uint flags) {
        _flags = flags;
    }

    void copy(MemGate *mem, uintptr_t virt) {
        // if we are the last one, we can just take the memory
        if(_mem->is_last()) {
            // we are the owner now
            _mem->owner_mem = mem;
            _mem->owner_virt = virt;
            return;
        }

        // make a copy; either from owner memory or the physical memory
        MemGate *ogate = _mem->owner_mem ? _mem->owner_mem : _mem->gate;
        uintptr_t off = _mem->owner_mem ? _mem->owner_virt : _memoff;
        MemGate *ngate = new MemGate(MemGate::create_global(size(), MemGate::RWX));
        copy_block(ogate, ngate, off + _offset, size());

        // are we the owner?
        if(mem == _mem->owner_mem) {
            // give the others the new memory gate
            MemGate *old = _mem->gate;
            _mem->gate = ngate;
            // there is no owner anymore
            _mem->owner_mem = nullptr;
            // give us the old memory with a new PhysMem object
            _mem = Reference<PhysMem>(new PhysMem(mem, old, _mem->owner_virt));
        }
        else
            _mem->gate = ngate;
    }

    void clear() {
        size_t pages = size() >> PAGE_BITS;
        for(size_t i = 0; i < pages; ++i)
            _mem->gate->write_sync(zeros, sizeof(zeros), i * PAGE_SIZE);
    }

    uintptr_t offset() const {
        return _offset;
    }
    void offset(uintptr_t offset) {
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
    Reference<PhysMem> _mem;
    uintptr_t _offset;
    size_t _memoff;
    size_t _size;
    uint _flags;
};

class RegionList {
public:
    typedef SList<Region>::iterator iterator;

    explicit RegionList(size_t total) : _total(total), _regs() {
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
        Region *n = new Region(start, end - start);
        _regs.insert(last, n);
        return n;
    }

    void print(OStream &os, uintptr_t virt) const {
        for(auto reg = _regs.begin(); reg != _regs.end(); ++reg) {
            os << "    " << fmt(virt + reg->offset(), "p");
            os << " .. " << fmt(virt + reg->offset() + reg->size() - 1, "p");
            os << " COW=" << ((reg->flags() & Region::COW) ? "1" : "0");
            os << " -> ";
            reg->mem()->print(os);
            os << "\n";
        }
    }

private:
    size_t _total;
    SList<Region> _regs;
};

}
