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

static char zeros[4096];
static char tmpbuf[4096];

static void copy_block(m3::MemGate *src, m3::MemGate *dst, size_t srcoff, size_t size) {
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
class PhysMem : public m3::RefCounted {
    friend class Region;

    explicit PhysMem(m3::MemGate *mem, m3::MemGate *gate, uintptr_t virt)
        : RefCounted(), gate(gate), owner_mem(mem), owner_virt(virt) {
    }

public:
    explicit PhysMem(m3::MemGate *mem, uintptr_t virt, size_t size, uint perm)
        : RefCounted(), gate(new m3::MemGate(m3::MemGate::create_global(size, perm))),
          owner_mem(mem), owner_virt(virt) {
    }
    explicit PhysMem(m3::MemGate *mem, uintptr_t virt, capsel_t sel)
        : RefCounted(), gate(new m3::MemGate(m3::MemGate::bind(sel))),
          owner_mem(mem), owner_virt(virt) {
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
    uintptr_t owner_virt;
};

/**
 * A region is a part of a dataspace, that allows us to allocate, copy, etc. smaller parts of the
 * dataspace.
 */
class Region : public m3::SListItem {
public:
    enum Flags {
        COW     = 1 << 0,
    };

    explicit Region(uintptr_t base, size_t offset, size_t size)
        : SListItem(), _mem(), _base(base), _offset(offset), _memoff(), _size(size), _flags() {
    }
    Region(const Region &r)
        : SListItem(r), _mem(r._mem), _base(r._base), _offset(r._offset), _memoff(r._memoff),
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
        _mem = m3::Reference<PhysMem>(mem);
    }

    uint flags() const {
        return _flags;
    }
    void flags(uint flags) {
        _flags = flags;
    }

    void copy(m3::MemGate *mem, uintptr_t virt) {
        // if we are the last one, we can just take the memory
        if(_mem->is_last()) {
            SLOG(PAGER, "Keeping memory "
                << m3::fmt(_base + _offset, "p") << ".."
                << m3::fmt(_base + _offset + size() - 1, "p"));

            // we are the owner now
            _mem->owner_mem = mem;
            _mem->owner_virt = virt;
            return;
        }

        // make a copy; either from owner memory or the physical memory
        m3::MemGate *ogate = _mem->owner_mem ? _mem->owner_mem : _mem->gate;
        uintptr_t off = _mem->owner_mem ? _mem->owner_virt : _memoff;
        m3::MemGate *ngate = new m3::MemGate(m3::MemGate::create_global(size(), m3::MemGate::RWX));

        SLOG(PAGER, "Copying memory "
            << m3::fmt(_base + _offset, "p") << ".."
            << m3::fmt(_base + _offset + size() - 1, "p")
            << " from " << (_mem->owner_mem ? "owner" : "origin"));

        copy_block(ogate, ngate, off + _offset, size());

        // are we the owner?
        if(mem == _mem->owner_mem) {
            // give the others the new memory gate
            m3::MemGate *old = _mem->gate;
            _mem->gate = ngate;
            // there is no owner anymore
            _mem->owner_mem = nullptr;
            // give us the old memory with a new PhysMem object
            _mem = m3::Reference<PhysMem>(new PhysMem(mem, old, _mem->owner_virt));
        }
        else
            _mem->gate = ngate;
    }

    void clear() {
        size_t pages = size() >> PAGE_BITS;
        for(size_t i = 0; i < pages; ++i)
            _mem->gate->write_sync(zeros, sizeof(zeros), i * PAGE_SIZE);
    }

    uintptr_t virt() const {
        return _base + _offset;
    }

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
    uintptr_t _base;
    size_t _offset;
    size_t _memoff;
    size_t _size;
    uint _flags;
};

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
