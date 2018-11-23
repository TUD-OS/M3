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
#include <base/util/String.h>
#include <base/util/Reference.h>

#include "mem/SlabCache.h"
#include "SendQueue.h"
#include "Gate.h"

namespace kernel {

class VPE;
class RGateObject;

class Service : public SlabObject<Service>, public m3::SListItem, public m3::RefCounted {
public:
    explicit Service(VPE &vpe, capsel_t sel, const m3::String &name, const m3::Reference<RGateObject> &rgate);
    ~Service();

    VPE &vpe() const {
        return _squeue.vpe();
    }
    capsel_t selector() const {
        return _sel;
    }
    const m3::String &name() const {
        return _name;
    }
    const m3::Reference<RGateObject> &rgate() const {
        return _rgate;
    }

    int pending() const;

    void send(const void *msg, size_t size, bool free);
    const m3::DTU::Message *send_receive(const void *msg, size_t size, bool free);
    void abort() {
        _squeue.abort();
    }

private:
    SendQueue _squeue;
    capsel_t _sel;
    m3::String _name;
    SendGate _sgate;
    m3::Reference<RGateObject> _rgate;
};

class ServiceList {
    explicit ServiceList() : _list() {
    }

public:
    friend class Service;

    using iterator = m3::SList<Service>::iterator;

    static ServiceList &get() {
        return _inst;
    }

    iterator begin() {
        return _list.begin();
    }
    iterator end() {
        return _list.end();
    }

    Service *add(VPE &vpe, capsel_t sel, const m3::String &name, const m3::Reference<RGateObject> &rgate) {
        Service *inst = new Service(vpe, sel, name, rgate);
        // prepend to the list to shutdown services in the opposite order
        _list.insert(nullptr, inst);
        return inst;
    }
    bool contains(const m3::String &name) const {
        return const_cast<ServiceList*>(this)->find(name) != nullptr;
    }
    Service *find(const m3::String &name) {
        for(auto &s : _list) {
            if(s.name() == name)
                return &s;
        }
        return nullptr;
    }

    void send(m3::Reference<Service> serv, const void *msg, size_t size, bool free) {
        serv->send(msg, size, free);
    }

private:
    void remove(Service *inst) {
        _list.remove(inst);
    }

    m3::SList<Service> _list;
    static ServiceList _inst;
};

}
