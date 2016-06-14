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

#include <m3/com/GateStream.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/LocList.h>

#include "DataSpace.h"

class AddrSpace : public m3::RequestSessionData {
public:
    explicit AddrSpace(AddrSpace *_parent = nullptr, capsel_t _sess = m3::ObjCap::INVALID)
       : RequestSessionData(),
         vpe(m3::ObjCap::VIRTPE, m3::ObjCap::INVALID),
         sess(m3::ObjCap::SESSION, _sess),
         mem(), dstree(), parent(_parent) {
    }
    ~AddrSpace() {
        for(auto ds = dslist.begin(); ds != dslist.end(); ) {
            auto old = ds++;
            delete &*old;
        }
        delete mem;
    }

    void add(DataSpace *ds) {
        dslist.append(ds);
        dstree.insert(ds);

        // if we manipulate the address space, cloning is no longer possible
        parent = nullptr;
    }

    void remove(DataSpace *ds) {
        dslist.remove(ds);
        dstree.remove(ds);
        parent = nullptr;
    }

    m3::Errors::Code clone() {
        if(!parent)
            return m3::Errors::NOT_SUP;

        // TODO handle the case where we already have mappings
        for(auto ds = parent->dslist.begin(); ds != parent->dslist.end(); ++ds) {
            DataSpace *dscopy = const_cast<DataSpace*>(&*ds)->clone(mem, parent->vpe.sel());
            dslist.append(dscopy);
            dstree.insert(dscopy);
        }

        // this can be done just once
        parent = nullptr;
        return m3::Errors::NO_ERROR;
    }

    const DataSpace *find(uintptr_t virt) const {
        return dstree.find(virt);
    }

    capsel_t init(capsel_t caps) {
        vpe = m3::ObjCap(m3::ObjCap::VIRTPE, caps + 0);
        mem = new m3::MemGate(m3::MemGate::bind(caps + 1));
        return vpe.sel();
    }

    void print(m3::OStream &os) const {
        for(auto ds = dslist.begin(); ds != dslist.end(); ++ds)
            ds->print(os);
    }

    m3::ObjCap vpe;
    m3::ObjCap sess;
    m3::MemGate *mem;
    m3::SList<DataSpace> dslist;
    m3::Treap<DataSpace> dstree;
    // TODO if the parent destroys his session first, we have a problem
    // atm, this basically can't happen because if the parent exits, the child is destroyed, too
    AddrSpace *parent;
    static int nextId;
};
