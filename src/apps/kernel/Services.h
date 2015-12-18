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
#include <m3/col/SList.h>
#include <m3/cap/SendGate.h>
#include <m3/cap/RecvGate.h>
#include <m3/util/String.h>
#include <m3/util/Reference.h>
#include <m3/Log.h>

#include "KSendQueue.h"

namespace m3 {

class KVPE;
class Gate;

class Service : public m3::SListItem, public RefCounted {
public:
    explicit Service(KVPE &vpe, int sel, const m3::String &name, capsel_t gate, int capacity)
        : m3::SListItem(), RefCounted(), closing(), _vpe(vpe), _sel(sel), _name(name),
          _sgate(m3::SendGate::bind(gate, nullptr, m3::ObjCap::KEEP_CAP)), _queue(capacity) {
    }
    ~Service();

    KVPE &vpe() const {
        return _vpe;
    }
    int selector() const {
        return _sel;
    }
    const m3::String &name() const {
        return _name;
    }
    m3::SendGate &send_gate() const {
        return const_cast<m3::SendGate&>(_sgate);
    }

    int pending() const {
        return _queue.inflight() + _queue.pending();
    }
    void send(RecvGate *rgate, const void *msg, size_t size) {
        _queue.send(rgate, &_sgate, msg, size);
    }
    void received_reply() {
        _queue.received_reply();
    }

    bool closing;

private:
    KVPE &_vpe;
    int _sel;
    m3::String _name;
    m3::SendGate _sgate;
    KSendQueue _queue;
};

class ServiceList {
    explicit ServiceList() : _list() {
    }

public:
    friend class Service;

    using iterator = SList<Service>::iterator;

    static ServiceList &get() {
        return _inst;
    }

    iterator begin() {
        return _list.begin();
    }
    iterator end() {
        return _list.end();
    }

    Service *add(KVPE &vpe, int sel, const m3::String &name, capsel_t gate, int capacity) {
        Service *inst = new Service(vpe, sel, name, gate, capacity);
        _list.append(inst);
        return inst;
    }
    Service *find(const m3::String &name) {
        for(auto &s : _list) {
            if(s.name() == name)
                return &s;
        }
        return nullptr;
    }
    void send_and_receive(Reference<Service> serv, const void *msg, size_t size);

private:
    void remove(Service *inst) {
        _list.remove(inst);
    }

    m3::SList<Service> _list;
    static ServiceList _inst;
};

}
