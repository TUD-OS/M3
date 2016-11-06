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
}

void AddrSpace::remove(DataSpace *ds) {
    dslist.remove(ds);
    dstree.remove(ds);
}

m3::Errors::Code AddrSpace::clone() {
    if(!parent)
        return m3::Errors::NOT_SUP;

    for(auto ds = parent->dslist.begin(); ds != parent->dslist.end(); ++ds) {
        DataSpace *cur = dstree.find(ds->addr());
        DataSpace *dscopy;

        // if the same dataspace does already exist, keep it and inherit it again
        if(cur && cur->id() == ds->id())
            dscopy = cur;
        else {
            // otherwise, if it existed, remove it
            if(cur) {
                remove(cur);
                delete cur;
            }
            // create a copy
            dscopy = const_cast<DataSpace*>(&*ds)->clone(this);
            dslist.append(dscopy);
            dstree.insert(dscopy);
        }

        // now do the inheritance, i.e. copy-on-write the regions
        dscopy->inherit(&*ds);
    }

    return m3::Errors::NONE;
}

capsel_t AddrSpace::init(capsel_t caps) {
    vpe = m3::ObjCap(m3::ObjCap::VIRTPE, caps + 0);
    mem = new m3::MemGate(m3::MemGate::bind(caps + 1));
    // we don't want to cause pagefault with this, because we are the one that handles them.
    // we will make sure that this doesn't happen by only accessing memory where we are sure that
    // we have mapped it.
    mem->cmdflags(m3::MemGate::CMD_NOPF);
    return vpe.sel();
}

void AddrSpace::print(m3::OStream &os) const {
    for(auto ds = dslist.begin(); ds != dslist.end(); ++ds)
        ds->print(os);
}
