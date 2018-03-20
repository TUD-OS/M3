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
#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>

#include "DataSpace.h"

class AddrSpace {
public:
    struct SGateItem : public m3::SListItem {
        explicit SGateItem(m3::SendGate &&_sgate) : sgate(m3::Util::move(_sgate)) {
        }
        m3::SendGate sgate;
    };

    explicit AddrSpace(AddrSpace *_parent = nullptr, capsel_t _sess = m3::ObjCap::INVALID)
       : alive(true), vpe(m3::ObjCap::VIRTPE, m3::ObjCap::INVALID),
         sess(m3::ObjCap::SESSION, _sess),
         mem(), sgates(), dstree(), parent(_parent) {
    }
    ~AddrSpace() {
        // don't revoke mapping caps on session destruction; the kernel will revoke them
        alive = false;
        for(auto ds = dslist.begin(); ds != dslist.end(); ) {
            auto old = ds++;
            delete &*old;
        }
        for(auto it = sgates.begin(); it != sgates.end(); ) {
            auto old = it++;
            delete &*old;
        }
        delete mem;
    }

    const DataSpace *find(goff_t virt) const {
        return dstree.find(virt);
    }

    capsel_t init(capsel_t caps);
    bool overlaps(goff_t virt, size_t size) const;
    void add(DataSpace *ds);
    void remove(DataSpace *ds);
    m3::Errors::Code clone();

    void print(m3::OStream &os) const;

    bool alive;
    m3::ObjCap vpe;
    m3::ObjCap sess;
    m3::MemGate *mem;
    m3::SList<SGateItem> sgates;
    m3::SList<DataSpace> dslist;
    m3::Treap<DataSpace> dstree;
    // TODO if the parent destroys his session first, we have a problem
    // atm, this basically can't happen because if the parent exits, the child is destroyed, too
    AddrSpace *parent;
    static int nextId;
};
