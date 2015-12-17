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
#include <m3/cap/MemGate.h>
#include <m3/DTU.h>

#include "Services.h"
#include "Treap.h"

namespace m3 {

class CapTable;
class Capability;
class OStream;

OStream &operator<<(OStream &os, const Capability &cc);

class Capability : public TreapNode<capsel_t> {
    friend class CapTable;

public:
    typedef capsel_t key_t;

    enum {
        SERVICE = 0x01,
        SESSION = 0x02,
        MSG     = 0x04,
        MEM     = 0x08,
        VPE     = 0x10,
    };

    explicit Capability(unsigned type)
        : TreapNode<capsel_t>(-1), type(type), _tbl(), _child(), _parent(), _next(), _prev() {
    }
    virtual ~Capability() {
    }

    capsel_t sel() const {
        return key();
    }
    CapTable *table() {
        return _tbl;
    }
    const CapTable *table() const {
        return _tbl;
    }
    Capability *next() {
        return _next;
    }
    const Capability *next() const {
        return _next;
    }
    Capability *child() {
        return _child;
    }
    const Capability *child() const {
        return _child;
    }

private:
    virtual Errors::Code revoke() {
        return Errors::NO_ERROR;
    }
    virtual Capability *clone() = 0;
    void put(CapTable *tbl, capsel_t sel) {
        _tbl = tbl;
        key(sel);
    }

public:
    unsigned type;

private:
    CapTable *_tbl;
    Capability *_child;
    Capability *_parent;
    Capability *_next;
    Capability *_prev;
};

class MsgObject : public RefCounted {
public:
    explicit MsgObject(label_t _label, int _core, int _epid, word_t _credits)
        : RefCounted(), label(_label), core(_core), epid(_epid), credits(_credits), derived(false) {
    }
    virtual ~MsgObject() {
    }

    label_t label;
    int core;
    int epid;
    word_t credits;
    bool derived;
};

class MemObject : public MsgObject {
public:
    explicit MemObject(uintptr_t addr, size_t size, uint perms, int core, int epid)
        : MsgObject(addr | perms, core, epid, size) {
        assert((addr & MemGate::RWX) == 0);
    }
    virtual ~MemObject();
};

class SessionObject : public RefCounted {
public:
    explicit SessionObject(Service *_srv, word_t _ident) : RefCounted(), ident(_ident), srv(_srv) {
    }
    ~SessionObject();

    word_t ident;
    Reference<Service> srv;
};

class MsgCapability : public Capability {
protected:
    explicit MsgCapability(unsigned type, MsgObject *_obj)
        : Capability(type), localepid(-1), obj(_obj) {
    }

public:
    explicit MsgCapability(label_t label, int core, int epid,
            word_t credits)
        : MsgCapability(MSG, new MsgObject(label, core, epid, credits)) {
    }

    void print(OStream &os) const override;

protected:
    virtual Errors::Code revoke() override;
    virtual Capability *clone() override {
        MsgCapability *c = new MsgCapability(*this);
        c->localepid = -1;
        return c;
    }

public:
    int localepid;
    Reference<MsgObject> obj;
};

class MemCapability : public MsgCapability {
public:
    explicit MemCapability(uintptr_t addr, size_t size, uint perms,
            int core, int epid)
        : MsgCapability(MEM | MSG, new MemObject(addr, size, perms, core, epid)) {
    }

    void print(OStream &os) const override;

    uintptr_t addr() const {
        return obj->label & ~MemGate::RWX;
    }
    size_t size() const {
        return obj->credits;
    }
    uint perms() const {
        return obj->label & MemGate::RWX;
    }

private:
    virtual Capability *clone() override {
        MemCapability *c = new MemCapability(*this);
        c->localepid = -1;
        return c;
    }
};

class ServiceCapability : public Capability {
public:
    explicit ServiceCapability(Service *_inst)
        : Capability(SERVICE), inst(_inst) {
    }

    void print(OStream &os) const override;

private:
    virtual Errors::Code revoke() override;
    virtual Capability *clone() override {
        /* not cloneable */
        return nullptr;
    }

public:
    Reference<Service> inst;
};

class SessionCapability : public Capability {
public:
    explicit SessionCapability(Service *srv, word_t ident)
        : Capability(SESSION), obj(new SessionObject(srv, ident)) {
    }

    void print(OStream &os) const override;

private:
    virtual Errors::Code revoke() override;
    virtual Capability *clone() override {
        return new SessionCapability(*this);
    }

public:
    Reference<SessionObject> obj;
};

class VPECapability : public Capability {
public:
    explicit VPECapability(KVPE *p);
    VPECapability(const VPECapability &t);

    void print(OStream &os) const override;

private:
    virtual Errors::Code revoke() override;
    virtual Capability *clone() override {
        return new VPECapability(*this);
    }

public:
    KVPE *vpe;
};

}
