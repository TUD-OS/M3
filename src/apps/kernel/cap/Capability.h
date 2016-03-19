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
#include <base/DTU.h>
#include <base/KIF.h>

#include "com/Services.h"

namespace m3 {
class OStream;
}

namespace kernel {

class CapTable;
class Capability;

m3::OStream &operator<<(m3::OStream &os, const Capability &cc);

class Capability : public m3::TreapNode<capsel_t> {
    friend class CapTable;

public:
    typedef capsel_t key_t;

    enum {
        SERVICE = 0x01,
        SESSION = 0x02,
        MSG     = 0x04,
        MEM     = 0x08,
        MAP     = 0x10,
        VIRTPE  = 0x20,
    };

    explicit Capability(CapTable *tbl, capsel_t sel, unsigned type)
        : TreapNode<capsel_t>(sel), type(type), _tbl(tbl), _child(), _parent(), _next(), _prev() {
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
    void put(CapTable *tbl, capsel_t sel) {
        _tbl = tbl;
        key(sel);
    }

private:
    virtual m3::Errors::Code revoke() {
        return m3::Errors::NO_ERROR;
    }
    virtual Capability *clone(CapTable *tbl, capsel_t sel) = 0;

public:
    unsigned type;

private:
    CapTable *_tbl;
    Capability *_child;
    Capability *_parent;
    Capability *_next;
    Capability *_prev;
};

class MsgObject : public m3::RefCounted {
public:
    explicit MsgObject(label_t _label, int _core, int _vpe, int _epid, word_t _credits)
        : RefCounted(), label(_label), core(_core), vpe(_vpe), epid(_epid), credits(_credits),
          derived(false) {
    }
    virtual ~MsgObject() {
    }

    label_t label;
    int core;
    int vpe;
    int epid;
    word_t credits;
    bool derived;
};

class MemObject : public MsgObject {
public:
    explicit MemObject(uintptr_t addr, size_t size, uint perms, int core, int vpe, int epid)
        : MsgObject(addr | perms, core, vpe, epid, size) {
        assert((addr & m3::KIF::Perm::RWX) == 0);
    }
    virtual ~MemObject();
};

class SessionObject : public m3::RefCounted {
public:
    explicit SessionObject(Service *_srv, word_t _ident) : RefCounted(), ident(_ident), srv(_srv) {
    }
    ~SessionObject();

    word_t ident;
    m3::Reference<Service> srv;
};

class MsgCapability : public Capability {
protected:
    explicit MsgCapability(CapTable *tbl, capsel_t sel, unsigned type, MsgObject *_obj)
        : Capability(tbl, sel, type), localepid(-1), obj(_obj) {
    }

public:
    explicit MsgCapability(CapTable *tbl, capsel_t sel, label_t label, int core, int vpe, int epid,
        word_t credits)
        : MsgCapability(tbl, sel, MSG, new MsgObject(label, core, vpe, epid, credits)) {
    }

    void print(m3::OStream &os) const override;

protected:
    virtual m3::Errors::Code revoke() override;
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        MsgCapability *c = new MsgCapability(*this);
        c->put(tbl, sel);
        c->localepid = -1;
        return c;
    }

public:
    int localepid;
    m3::Reference<MsgObject> obj;
};

class MemCapability : public MsgCapability {
public:
    explicit MemCapability(CapTable *tbl, capsel_t sel, uintptr_t addr, size_t size, uint perms,
            int core, int vpe, int epid)
        : MsgCapability(tbl, sel, MEM | MSG, new MemObject(addr, size, perms, core, vpe, epid)) {
    }

    void print(m3::OStream &os) const override;

    uintptr_t addr() const {
        return obj->label & ~m3::KIF::Perm::RWX;
    }
    size_t size() const {
        return obj->credits;
    }
    uint perms() const {
        return obj->label & m3::KIF::Perm::RWX;
    }

private:
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        MemCapability *c = new MemCapability(*this);
        c->put(tbl, sel);
        c->localepid = -1;
        return c;
    }
};

class MapCapability : public Capability {
public:
    explicit MapCapability(CapTable *tbl, capsel_t sel, uintptr_t _phys, uint _attr);

    void print(m3::OStream &os) const override;

private:
    virtual m3::Errors::Code revoke() override;
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        return new MapCapability(tbl, sel, phys, attr);
    }

public:
    uintptr_t phys;
    uint attr;
};

class ServiceCapability : public Capability {
public:
    explicit ServiceCapability(CapTable *tbl, capsel_t sel, Service *_inst)
        : Capability(tbl, sel, SERVICE), inst(_inst) {
    }

    void print(m3::OStream &os) const override;

private:
    virtual m3::Errors::Code revoke() override;
    virtual Capability *clone(CapTable *, capsel_t) override {
        /* not cloneable */
        return nullptr;
    }

public:
    m3::Reference<Service> inst;
};

class SessionCapability : public Capability {
public:
    explicit SessionCapability(CapTable *tbl, capsel_t sel, Service *srv, word_t ident)
        : Capability(tbl, sel, SESSION), obj(new SessionObject(srv, ident)) {
    }

    void print(m3::OStream &os) const override;

private:
    virtual m3::Errors::Code revoke() override;
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        SessionCapability *s = new SessionCapability(*this);
        s->put(tbl, sel);
        return s;
    }

public:
    m3::Reference<SessionObject> obj;
};

class VPECapability : public Capability {
public:
    explicit VPECapability(CapTable *tbl, capsel_t sel, VPE *p);
    VPECapability(const VPECapability &t);

    void print(m3::OStream &os) const override;

private:
    virtual m3::Errors::Code revoke() override;
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        VPECapability *v = new VPECapability(*this);
        v->put(tbl, sel);
        return v;
    }

public:
    VPE *vpe;
};

}
