/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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
#include <m3/Log.h>

#include "Services.h"
#include "Capability.h"

namespace m3 {

class CapTable;

OStream &operator<<(OStream &os, const CapTable &ct);

class CapTable {
    friend OStream &operator<<(OStream &os, const CapTable &ct);

    static constexpr size_t MAX_ENTRIES    = VPE::SEL_TOTAL;
public:
    static CapTable &kernel_table() {
        return _kcaps;
    }

    explicit CapTable(uint id) : _id(id), _caps() {
    }
    CapTable(const CapTable &ct, uint id) : _id(id), _caps() {
        for(size_t i = 0; i < MAX_ENTRIES; ++i) {
            if(ct._caps[i])
                _caps[i] = ct._caps[i]->clone();
        }
    }
    ~CapTable() {
        revoke_all();
    }

    uint id() const {
        return _id;
    }
    bool valid(capsel_t i) const {
        return i < MAX_ENTRIES;
    }
    bool unused(capsel_t i) const {
        return valid(i) && _caps[i] == nullptr;
    }
    bool used(capsel_t i) const {
        return valid(i) && _caps[i] != nullptr;
    }
    bool range_unused(const m3::CapRngDesc &crd) const {
        if(!range_valid(crd))
            return false;
        for(capsel_t i = crd.start(); i < crd.start() + crd.count(); ++i) {
            if(_caps[i] != nullptr)
                return false;
        }
        return true;
    }
    bool range_used(const m3::CapRngDesc &crd) const {
        if(!range_valid(crd))
            return false;
        for(capsel_t i = crd.start(); i < crd.start() + crd.count(); ++i) {
            if(_caps[i] == nullptr)
                return false;
        }
        return true;
    }

    Capability *obtain(capsel_t dst, Capability *c);
    void inherit(Capability *parent, Capability *child);
    Errors::Code revoke(const m3::CapRngDesc &crd);

    Capability *get(capsel_t i) {
        if(!valid(i))
            return nullptr;
        return _caps[i];
    }
    Capability *get(capsel_t i, unsigned types) {
        if(!valid(i) || _caps[i] == nullptr || !(_caps[i]->type & types))
            return nullptr;
        return _caps[i];
    }

    void set(capsel_t i, Capability *c) {
        assert(_caps[i] == nullptr);
        if(c) {
            c->put(this, i);
            LOG(CAPS, "CapTable[" << _id << "]: Setting " << i << " to " << *c);
        }
        else
            LOG(CAPS, "CapTable[" << _id << "]: Setting " << i << " to NULL");
        _caps[i] = c;
    }
    void unset(capsel_t i) {
        LOG(CAPS, "CapTable[" << _id << "]: Unsetting " << i);
        delete _caps[i];
        _caps[i] = nullptr;
    }

    void revoke_all();

private:
    static Errors::Code revoke_rec(Capability *c, bool revnext);
    static void print_rec(OStream &os, int level, const Capability *c);
    bool range_valid(const m3::CapRngDesc &crd) const {
        return crd.count() == 0 || (crd.start() + crd.count() > crd.start() && valid(crd.start())
                && valid(crd.start() + crd.count() - 1));
    }

    uint _id;
    Capability *_caps[MAX_ENTRIES];
    static CapTable _kcaps;
};

}
