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

#include <m3/Common.h>
#include <m3/cap/VPE.h>
#include <m3/CapRngDesc.h>
#include <m3/col/Treap.h>
#include <m3/Log.h>

#include "Services.h"
#include "Capability.h"

namespace m3 {

class CapTable;

OStream &operator<<(OStream &os, const CapTable &ct);

class CapTable {
    friend OStream &operator<<(OStream &os, const CapTable &ct);

public:
    static CapTable &kernel_table() {
        return _kcaps;
    }

    explicit CapTable(uint id) : _id(id), _caps() {
    }
    CapTable(const CapTable &ct, uint id) = delete;
    ~CapTable() {
        revoke_all();
    }

    uint id() const {
        return _id;
    }
    bool unused(capsel_t i) const {
        return get(i) == nullptr;
    }
    bool used(capsel_t i) const {
        return get(i) != nullptr;
    }
    bool range_unused(const m3::CapRngDesc &crd) const {
        if(!range_valid(crd))
            return false;
        for(capsel_t i = crd.start(); i < crd.start() + crd.count(); ++i) {
            if(get(i) != nullptr)
                return false;
        }
        return true;
    }
    bool range_used(const m3::CapRngDesc &crd) const {
        if(!range_valid(crd))
            return false;
        for(capsel_t i = crd.start(); i < crd.start() + crd.count(); ++i) {
            if(get(i) == nullptr)
                return false;
        }
        return true;
    }

    Capability *obtain(capsel_t dst, Capability *c);
    void inherit(Capability *parent, Capability *child);
    Errors::Code revoke(const m3::CapRngDesc &crd);

    Capability *get(capsel_t i) {
        return _caps.find(i);
    }
    const Capability *get(capsel_t i) const {
        return _caps.find(i);
    }
    Capability *get(capsel_t i, unsigned types) {
        Capability *c = get(i);
        if(c == nullptr || !(c->type & types))
            return nullptr;
        return c;
    }

    void set(capsel_t i, Capability *c) {
        assert(get(i) == nullptr);
        if(c) {
            assert(c->table() == this);
            assert(c->sel() == i);
            _caps.insert(c);
            LOG(CAPS, "CapTable[" << _id << "]: Setting " << i << " to " << *c);
        }
        else
            LOG(CAPS, "CapTable[" << _id << "]: Setting " << i << " to NULL");
    }
    void unset(capsel_t i) {
        LOG(CAPS, "CapTable[" << _id << "]: Unsetting " << i);
        Capability *c = get(i);
        if(c) {
            _caps.remove(c);
            delete c;
        }
    }

    void revoke_all();

private:
    static Errors::Code revoke(Capability *c);
    static Errors::Code revoke_rec(Capability *c, bool revnext);
    bool range_valid(const m3::CapRngDesc &crd) const {
        return crd.count() == 0 || crd.start() + crd.count() > crd.start();
    }

    uint _id;
    Treap<Capability> _caps;
    static CapTable _kcaps;
};

}
