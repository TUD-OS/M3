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
#include <base/Log.h>

#include "SendQueue.h"
#include "Gate.h"

namespace kernel {

class VPE;

class Service : public m3::SListItem, public m3::RefCounted {
public:
    explicit Service(VPE &vpe, int sel, const m3::String &name, int ep, label_t label, int capacity)
        : m3::SListItem(), RefCounted(), closing(), _vpe(vpe), _sel(sel), _name(name),
          _sgate(vpe, ep, label), _queue(capacity) {
    }
    ~Service();

    VPE &vpe() const {
        return _vpe;
    }
    int selector() const {
        return _sel;
    }
    const m3::String &name() const {
        return _name;
    }
    SendGate &send_gate() const {
        return const_cast<SendGate&>(_sgate);
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
    VPE &_vpe;
    int _sel;
    m3::String _name;
    SendGate _sgate;
    SendQueue _queue;
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

    Service *add(VPE &vpe, int sel, const m3::String &name, int ep, label_t label, int capacity) {
        Service *inst = new Service(vpe, sel, name, ep, label, capacity);
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
    void send_and_receive(m3::Reference<Service> serv, const void *msg, size_t size);

private:
    void remove(Service *inst) {
        _list.remove(inst);
    }

    m3::SList<Service> _list;
    static ServiceList _inst;
};

}
