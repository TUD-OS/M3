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

#include <m3/Syscalls.h>

#include <thread/ThreadManager.h>

#include "AddrSpace.h"
#include "DataSpace.h"
#include "Region.h"

alignas(64) static char zeros[4096];
alignas(64) static char tmpbuf[4096];

void copy_block(m3::MemGate *src, m3::MemGate *dst, size_t srcoff, size_t size) {
    size_t pages = size >> PAGE_BITS;
    for(size_t i = 0; i < pages; ++i) {
        src->read (tmpbuf, sizeof(tmpbuf), srcoff + i * PAGE_SIZE);
        dst->write(tmpbuf, sizeof(tmpbuf), i * PAGE_SIZE);
    }
}

Region::~Region() {
    // if another address space still uses this, we still want to unmap it from this one
    if(_ds->addrspace_alive() && _mapped && has_mem() && !_mem->is_last()) {
        m3::Syscalls::get().revoke(_ds->addrspace()->vpe.sel(),
            m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::MAP, virt() >> PAGE_BITS, size() >> PAGE_BITS));
    }
}

goff_t Region::virt() const {
    return _ds->addr() + _offset;
}

void Region::limit_to(size_t pos, size_t pages) {
    if(size() > pages * PAGE_SIZE) {
        goff_t end = offset() + size();
        if(pos > (pages / 2) * PAGE_SIZE)
            offset(m3::Math::max(offset(), pos - (pages / 2) * PAGE_SIZE));
        size(m3::Math::min(static_cast<goff_t>(pages * PAGE_SIZE), end - offset()));
    }
}

m3::Errors::Code Region::map(int flags) {
    if(has_mem()) {
        _mapped = true;
        return m3::Syscalls::get().createmap(virt() >> PAGE_BITS,
            _ds->addrspace()->vpe.sel(), mem()->gate->sel(),
            mem_offset() >> PAGE_BITS, size() >> PAGE_BITS, flags);
    }
    return m3::Errors::NONE;
}

void Region::copy(m3::MemGate *mem, goff_t virt) {
    if(_copying)
        m3::ThreadManager::get().wait_for(reinterpret_cast<event_t>(this));

    // if we are the last one, we can just take the memory
    if(_mem->is_last()) {
        SLOG(PAGER, "Keeping memory "
            << m3::fmt(_ds->addr() + _offset, "p") << ".."
            << m3::fmt(_ds->addr() + _offset + size() - 1, "p"));

        // we are the owner now
        _mem->owner_mem = mem;
        _mem->owner_virt = virt;
        return;
    }

    // make a copy; either from owner memory or the physical memory
    m3::MemGate *ogate = _mem->owner_mem ? _mem->owner_mem : _mem->gate;
    goff_t off = _mem->owner_mem ? _mem->owner_virt : _memoff;
    m3::MemGate *ngate = new m3::MemGate(m3::MemGate::create_global(size(), m3::MemGate::RWX));

    SLOG(PAGER, "Copying memory "
        << m3::fmt(_ds->addr() + _offset, "p") << ".."
        << m3::fmt(_ds->addr() + _offset + size() - 1, "p")
        << " from " << (_mem->owner_mem ? "owner" : "origin")
        << " (we are " << (mem == _mem->owner_mem ? "owner" : "not owner") << ")");

    // if we copy from owner memory, this may require a forward and thus cause a thread switch
    // make sure that the other users of this memory don't continue (in copy()) until this is done
    // TODO if the owner unmaps the memory, we have a problem
    _copying = true;
    copy_block(ogate, ngate, off + _offset, size());
    _copying = false;
    m3::ThreadManager::get().notify(reinterpret_cast<event_t>(this));

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
    else {
        // the others keep the old mem; we take the new one
        _mem = m3::Reference<PhysMem>(new PhysMem(mem, ngate, virt));
    }
}

void Region::clear() {
    size_t pages = size() >> PAGE_BITS;
    for(size_t i = 0; i < pages; ++i)
        _mem->gate->write(zeros, sizeof(zeros), i * PAGE_SIZE);
}
