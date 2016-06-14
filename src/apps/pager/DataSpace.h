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
#include <base/col/Treap.h>
#include <base/stream/OStream.h>
#include <base/util/CapRngDesc.h>
#include <base/DTU.h>
#include <base/Errors.h>

#include <m3/com/MemGate.h>
#include <m3/session/Session.h>
#include <m3/session/M3FS.h>
#include <m3/Syscalls.h>

#include "RegionList.h"

namespace m3 {

class DataSpace : public TreapNode<uintptr_t>, public SListItem {
public:
    explicit DataSpace(MemGate *virt, uintptr_t addr, size_t size, uint flags)
        : TreapNode<uintptr_t>(addr), SListItem(), _flags(flags), _virt(virt),
          _regs(addr, size), _size(size) {
    }

    bool matches(uintptr_t k) override {
        return k >= addr() && k < addr() + _size;
    }

    uint flags() const {
        return _flags;
    }
    uintptr_t addr() const {
        return key();
    }
    size_t size() const {
        return _size;
    }

    DataSpace *clone(MemGate *virt, capsel_t srcvpe) {
        DataSpace *ds = do_clone(virt);

        // clone regions
        for(auto reg = _regs.begin(); reg != _regs.end(); ++reg) {
            // remove the writable bit if it was writable
            if(reg->has_mem() && (_flags & DTU::PTE_W)) {
                Syscalls::get().createmap(srcvpe, reg->mem()->gate->sel(),
                    reg->mem_offset() >> PAGE_BITS, reg->size() >> PAGE_BITS,
                    reg->virt() >> PAGE_BITS, _flags & ~DTU::PTE_W);
            }

            Region *nreg = new Region(*reg);
            ds->_regs.append(nreg);

            // adjust flags
            if(_flags & DTU::PTE_W)
                reg->flags(reg->flags() | Region::COW);
            // for the clone, even readonly regions are mapped on demand
            nreg->flags(nreg->flags() | Region::COW);
        }
        return ds;
    }

    virtual const char *type() const = 0;
    virtual Errors::Code get_page(uintptr_t *virt, int *pageNo, size_t *pages, capsel_t *sel) = 0;

    void print(OStream &os) const override {
        os << "  " << type() << "DataSpace["
           << "addr=" << fmt(addr(), "p")
           << ", size=" << fmt(size(), "#x")
           << ", flags=" << fmt(_flags, "#x") << "]:\n";
        _regs.print(os);
    }

protected:
    virtual DataSpace *do_clone(MemGate *virt) = 0;

    uint _flags;
    MemGate *_virt;
    RegionList _regs;
    size_t _size;
};

class AnonDataSpace : public DataSpace {
public:
    static constexpr size_t MAX_PAGES = 4;

    explicit AnonDataSpace(MemGate *virt, uintptr_t addr, size_t size, uint flags)
        : DataSpace(virt, addr, size, flags) {
    }

    const char *type() const override {
        return "Anon";
    }
    DataSpace *do_clone(MemGate *virt) override {
        return new AnonDataSpace(virt, addr(), size(), _flags);
    }

    Errors::Code get_page(uintptr_t *vaddr, int *pageNo, size_t *pages, capsel_t *sel) override {
        size_t offset = Math::round_dn(*vaddr - addr(), PAGE_SIZE);
        Region *reg = _regs.pagefault(offset);

        // if it isn't backed with memory yet, allocate memory for it
        if(!reg->has_mem()) {
            // don't allocate too much at once
            if(reg->size() > MAX_PAGES * PAGE_SIZE) {
                uintptr_t end = reg->offset() + reg->size();
                if(offset > (MAX_PAGES / 2) * PAGE_SIZE)
                    reg->offset(offset - (MAX_PAGES / 2) * PAGE_SIZE);
                else
                    reg->offset(0);
                reg->size(Math::min(reg->offset() + MAX_PAGES * PAGE_SIZE, end - reg->offset()));
            }

            SLOG(PAGER, "Allocating anonymous memory for "
                << fmt(reg->virt(), "p") << ".."
                << fmt(reg->virt() + reg->size() - 1, "p"));

            reg->mem(new PhysMem(_virt, addr(), reg->size(), MemGate::RWX));
            // zero the memory
            reg->clear();
        }
        // if we have memory, but COW is in progress
        else if(reg->flags() & Region::COW) {
            // writable memory needs to be copied
            if(_flags & DTU::PTE_W)
                reg->copy(_virt, addr());
            reg->flags(reg->flags() & ~Region::COW);
        }
        else {
            // otherwise, there is nothing to do
            *pages = 0;
            return Errors::NO_ERROR;
        }

        // we want to map the entire region
        *pageNo = 0;
        *pages = reg->size() >> PAGE_BITS;
        *vaddr = reg->virt();
        *sel = reg->mem()->gate->sel();
        return Errors::NO_ERROR;
    }
};

class ExternalDataSpace : public DataSpace {
public:
    explicit ExternalDataSpace(MemGate *virt, uintptr_t addr, size_t size, uint flags, int _id,
            size_t _offset, capsel_t sess)
        : DataSpace(virt, addr, size, flags), sess(sess), id(_id), offset(_offset) {
    }
    explicit ExternalDataSpace(MemGate *virt, uintptr_t addr, size_t size, uint flags, int _id,
            size_t _offset)
        : DataSpace(virt, addr, size, flags), sess(VPE::self().alloc_cap()), id(_id), offset(_offset) {
    }

    const char *type() const override {
        return "External";
    }
    DataSpace *do_clone(MemGate *virt) override {
        return new ExternalDataSpace(virt, addr(), size(), _flags, id, offset, sess.sel());
    }

    Errors::Code get_page(uintptr_t *vaddr, int *pageNo, size_t *pages, capsel_t *sel) override {
        // find the region
        Region *reg = _regs.pagefault(*vaddr - addr());

        off_t off = 0;
        // if we don't have memory yet, request it
        if(!reg->has_mem()) {
            CapRngDesc crd;
            loclist_type locs;
            // get memory caps for the region
            {
                size_t count = 1, blocks = 0;
                auto args = create_vmsg(id, offset + reg->offset(), count, blocks, M3FS::BYTE_OFFSET);
                GateIStream resp = sess.obtain(1, crd, args);
                if(Errors::last != Errors::NO_ERROR)
                    return Errors::last;

                // adjust region
                bool extended;
                resp >> locs >> extended >> off;
            }

            // if it's writable, create a copy
            // TODO let the mapper decide what to do (for m3fs, we get direct access to the file's
            // data, so that we have to copy that. but maybe this is not always the case)
            size_t sz = Math::round_up(locs.get(0) - off, PAGE_SIZE);
            if(_flags & DTU::PTE_W) {
                MemGate src(MemGate::bind(crd.start(), 0));
                reg->mem(new PhysMem(_virt, addr(), sz, MemGate::RWX));
                copy_block(&src, reg->mem()->gate, off, sz);
                off = 0;
            }
            else
                reg->mem(new PhysMem(_virt, addr(), crd.start()));

            // adjust size and store offset of the stuff we want to map within that mem cap
            if(sz < reg->size())
                reg->size(sz);
            reg->mem_offset(off);

            SLOG(PAGER, "Obtained memory for "
                << fmt(reg->virt(), "p") << ".."
                << fmt(reg->virt() + reg->size() - 1, "p"));
        }
        // handle copy on write
        else if(reg->flags() & Region::COW) {
            if(_flags & DTU::PTE_W)
                reg->copy(_virt, addr());
            reg->flags(reg->flags() & ~Region::COW);
            off = reg->mem_offset();
        }
        else {
            // otherwise, there is nothing to do
            *pages = 0;
            return Errors::NO_ERROR;
        }

        // that's what we want to map
        *pageNo = off >> PAGE_BITS;
        *pages = reg->size() >> PAGE_BITS;
        *vaddr = reg->virt();
        *sel = reg->mem()->gate->sel();
        return Errors::NO_ERROR;
    }

    Session sess;
    int id;
    size_t offset;
};

}
