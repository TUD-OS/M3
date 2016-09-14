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
#include <base/col/Treap.h>
#include <base/KIF.h>

#include "com/Services.h"
#include "cap/Capability.h"

namespace kernel {

class CapTable;

m3::OStream &operator<<(m3::OStream &os, const CapTable &ct);

class CapTable {
    friend m3::OStream &operator<<(m3::OStream &os, const CapTable &ct);

public:
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
    bool range_unused(const m3::KIF::CapRngDesc &crd) const {
        if(!range_valid(crd))
            return false;
        for(capsel_t i = crd.start(); i < crd.start() + crd.count(); ++i) {
            if(get(i) != nullptr)
                return false;
        }
        return true;
    }
    bool range_used(const m3::KIF::CapRngDesc &crd) const {
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
    m3::Errors::Code revoke(const m3::KIF::CapRngDesc &crd, bool own);

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

    void set(UNUSED capsel_t i, Capability *c) {
        assert(get(i) == nullptr);
        if(c) {
            assert(c->table() == this);
            assert(c->sel() == i);
            _caps.insert(c);
        }
    }
    void unset(capsel_t i) {
        Capability *c = get(i);
        if(c) {
            _caps.remove(c);
            delete c;
        }
    }

    void revoke_all();

#if defined(__t3__)
    void activate_msgcaps();
#endif

private:
    static m3::Errors::Code revoke(Capability *c, bool revnext);
    static m3::Errors::Code revoke_rec(Capability *c, bool revnext);
    bool range_valid(const m3::KIF::CapRngDesc &crd) const {
        return crd.count() == 0 || crd.start() + crd.count() > crd.start();
    }

    uint _id;
    m3::Treap<Capability> _caps;
};

}
