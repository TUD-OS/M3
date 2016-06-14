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

#include "AddrSpace.h"

int AddrSpace::nextId = 1;

void AddrSpace::add(DataSpace *ds) {
    dslist.append(ds);
    dstree.insert(ds);

    // if we manipulate the address space, cloning is no longer possible
    parent = nullptr;
}

void AddrSpace::remove(DataSpace *ds) {
    dslist.remove(ds);
    dstree.remove(ds);
    parent = nullptr;
}

m3::Errors::Code AddrSpace::clone() {
    if(!parent)
        return m3::Errors::NOT_SUP;

    // TODO handle the case where we already have mappings
    for(auto ds = parent->dslist.begin(); ds != parent->dslist.end(); ++ds) {
        DataSpace *dscopy = const_cast<DataSpace*>(&*ds)->clone(this);
        dslist.append(dscopy);
        dstree.insert(dscopy);
    }

    // this can be done just once
    parent = nullptr;
    return m3::Errors::NO_ERROR;
}

capsel_t AddrSpace::init(capsel_t caps) {
    vpe = m3::ObjCap(m3::ObjCap::VIRTPE, caps + 0);
    mem = new m3::MemGate(m3::MemGate::bind(caps + 1));
    return vpe.sel();
}

void AddrSpace::print(m3::OStream &os) const {
    for(auto ds = dslist.begin(); ds != dslist.end(); ++ds)
        ds->print(os);
}
