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

#include <m3/com/MemGate.h>
#include <m3/session/Session.h>
#include <m3/session/M3FS.h>
#include <m3/Syscalls.h>

#include "AddrSpace.h"
#include "DataSpace.h"

ulong DataSpace::_next_id = 0;

void DataSpace::inherit(DataSpace *ds) {
    _id = ds->_id;

    // if it's not writable, but we have already regions, we can simply keep them
    if(!(ds->_flags & m3::DTU::PTE_W) && _regs.count() > 0)
        return;

    // for the case that we already have regions and the DS is writable, just remove them. because
    // there is no point in trying to keep them:
    // 1. we have already our own copy
    //    -> then we need to revoke that and create a new one anyway
    // 2. COW is still set
    //    -> then we would save the object copying, but this is not that expensive
    // in general, if we try to keep them, we need to match the region lists against each other,
    // which is probably more expensive than just destructing and creating a few objects
    _regs.clear();

    // clone regions
    for(auto reg = ds->_regs.begin(); reg != ds->_regs.end(); ++reg) {
        // make it readonly, if it's writable and we have not done that yet
        if(!(reg->flags() & Region::COW) && ds->_flags & m3::DTU::PTE_W)
            reg->map(ds->_flags & ~m3::DTU::PTE_W);

        Region *nreg = new Region(*reg);
        nreg->ds(this);
        _regs.append(nreg);

        // adjust flags
        if(ds->_flags & m3::DTU::PTE_W)
            reg->flags(reg->flags() | Region::COW);
        // for the clone, even readonly regions are mapped on demand
        nreg->flags(nreg->flags() | Region::COW);
    }
}

void DataSpace::print(m3::OStream &os) const {
    os << "  " << type() << "DataSpace["
       << "addr=" << m3::fmt(addr(), "p")
       << ", size=" << m3::fmt(size(), "#x")
       << ", flags=" << m3::fmt(_flags, "#x") << "]:\n";
    _regs.print(os);
}

m3::Errors::Code AnonDataSpace::handle_pf(uintptr_t vaddr) {
    size_t offset = m3::Math::round_dn(vaddr - addr(), PAGE_SIZE);
    Region *reg = _regs.pagefault(offset);

    // if it isn't backed with memory yet, allocate memory for it
    if(!reg->has_mem()) {
        // don't allocate too much at once
        if(reg->size() > MAX_PAGES * PAGE_SIZE) {
            uintptr_t end = reg->offset() + reg->size();
            if(offset > (MAX_PAGES / 2) * PAGE_SIZE)
                reg->offset(m3::Math::max(reg->offset(), offset - (MAX_PAGES / 2) * PAGE_SIZE));
            else
                reg->offset(0);
            reg->size(m3::Math::min(MAX_PAGES * PAGE_SIZE, end - reg->offset()));
        }

        SLOG(PAGER, "Allocating anonymous memory for "
            << m3::fmt(reg->virt(), "p") << ".."
            << m3::fmt(reg->virt() + reg->size() - 1, "p"));

        reg->mem(new PhysMem(_as->mem, addr(), reg->size(), m3::MemGate::RWX));
        // zero the memory
        reg->clear();
    }
    // if we have memory, but COW is in progress
    else if(reg->flags() & Region::COW) {
        // writable memory needs to be copied
        if(_flags & m3::DTU::PTE_W)
            reg->copy(_as->mem, addr());
        reg->flags(reg->flags() & ~Region::COW);
    }
    else {
        // otherwise, there is nothing to do
        return m3::Errors::NO_ERROR;
    }

    return reg->map(flags());
}

m3::Errors::Code ExternalDataSpace::handle_pf(uintptr_t vaddr) {
    // find the region
    Region *reg = _regs.pagefault(vaddr - addr());

    // if we don't have memory yet, request it
    if(!reg->has_mem()) {
        off_t off;
        m3::CapRngDesc crd;
        m3::loclist_type locs;
        // get memory caps for the region
        {
            size_t count = 1, blocks = 0;
            auto args = m3::create_vmsg(id, offset + reg->offset(), count,
                blocks, m3::M3FS::BYTE_OFFSET);
            m3::GateIStream resp = sess.obtain(1, crd, args);
            if(m3::Errors::last != m3::Errors::NO_ERROR)
                return m3::Errors::last;

            // adjust region
            bool extended;
            resp >> locs >> extended >> off;
        }

        // if it's writable, create a copy
        // TODO let the mapper decide what to do (for m3fs, we get direct access to the file's
        // data, so that we have to copy that. but maybe this is not always the case)
        size_t sz = m3::Math::round_up(locs.get(0) - off, PAGE_SIZE);
        if(_flags & m3::DTU::PTE_W) {
            m3::MemGate src(m3::MemGate::bind(crd.start(), 0));
            reg->mem(new PhysMem(_as->mem, addr(), sz, m3::MemGate::RWX));
            copy_block(&src, reg->mem()->gate, off, sz);
            off = 0;
        }
        else
            reg->mem(new PhysMem(_as->mem, addr(), crd.start()));

        // adjust size and store offset of the stuff we want to map within that mem cap
        if(sz < reg->size())
            reg->size(sz);
        reg->mem_offset(off);

        SLOG(PAGER, "Obtained memory for "
            << m3::fmt(reg->virt(), "p") << ".."
            << m3::fmt(reg->virt() + reg->size() - 1, "p"));
    }
    // handle copy on write
    else if(reg->flags() & Region::COW) {
        if(_flags & m3::DTU::PTE_W)
            reg->copy(_as->mem, addr());
        reg->flags(reg->flags() & ~Region::COW);
    }
    else {
        // otherwise, there is nothing to do
        return m3::Errors::NO_ERROR;
    }

    return reg->map(flags());
}

