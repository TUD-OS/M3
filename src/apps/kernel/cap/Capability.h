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

#include "com/Services.h"
#include "mem/SlabCache.h"
#include "Types.h"

namespace m3 {
class OStream;
}

namespace kernel {

class CapTable;
class Capability;
class EPObject;
class GateObject;

m3::OStream &operator<<(m3::OStream &os, const Capability &cc);

class Capability : public m3::TreapNode<Capability, capsel_t> {
    friend class CapTable;

public:
    typedef capsel_t key_t;

    enum {
        SERV    = 0x01,
        SESS    = 0x02,
        SGATE   = 0x04,
        RGATE   = 0x08,
        MGATE   = 0x10,
        MAP     = 0x20,
        VIRTPE  = 0x40,
        EP      = 0x80,
    };

    explicit Capability(CapTable *tbl, capsel_t sel, unsigned type, uint len = 1)
        : TreapNode(sel),
          type(type),
          length(len),
          _tbl(tbl),
          _child(),
          _parent(),
          _next(),
          _prev() {
    }
    virtual ~Capability() {
    }

    bool matches(key_t key) {
        return key >= sel() && key < sel() + length;
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

    void print(m3::OStream &os) const;
    virtual void printInfo(m3::OStream &os) const = 0;
    void printChilds(m3::OStream &os, size_t layer = 0) const;

    virtual GateObject *as_gate() {
        return nullptr;
    }

private:
    virtual void revoke() {
    }
    virtual Capability *clone(CapTable *tbl, capsel_t sel) = 0;

public:
    uint type;
    uint length;

private:
    CapTable *_tbl;
    Capability *_child;
    Capability *_parent;
    Capability *_next;
    Capability *_prev;
};

class GateObject {
public:
    struct EPUser : public SlabObject<EPUser>, public m3::SListItem {
        explicit EPUser(EPObject *_ep)
            : m3::SListItem(),
              ep(_ep) {
        }
        EPObject *ep;
    };

    explicit GateObject(uint _type)
        : type(_type),
          epuser() {
    }
    ~GateObject();

    EPObject *ep_of_vpe(vpeid_t vpe);

    void add_ep(EPObject *ep) {
        epuser.append(new EPUser(ep));
    }
    void remove_ep(EPObject *ep) {
        delete epuser.remove_if([ep](EPUser *u) { return u->ep == ep; });
    }

    void print_eps(m3::OStream &os);

    uint type;
    m3::SList<EPUser> epuser;
};

class RGateObject : public SlabObject<RGateObject>, public GateObject, public m3::RefCounted {
public:
    explicit RGateObject(int _order, int _msgorder)
        : GateObject(Capability::RGATE),
          RefCounted(),
          vpe(),
          ep(),
          addr(),
          order(_order),
          msgorder(_msgorder),
          header() {
    }
    ~RGateObject();

    bool activated() const {
        return addr != 0;
    }
    size_t size() const {
        return 1UL << order;
    }

    vpeid_t vpe;
    epid_t ep;
    goff_t addr;
    int order;
    int msgorder;
    uint header;
};

class SGateObject : public SlabObject<SGateObject>, public GateObject, public m3::RefCounted {
public:
    explicit SGateObject(RGateObject *_rgate, label_t _label, word_t _credits)
        : GateObject(Capability::SGATE),
          RefCounted(),
          rgate(_rgate),
          label(_label),
          credits(_credits),
          activated() {
    }

    m3::Reference<RGateObject> rgate;
    label_t label;
    word_t credits;
    bool activated;
};

class MGateObject : public SlabObject<MGateObject>, public GateObject, public m3::RefCounted {
public:
    explicit MGateObject(peid_t _pe, vpeid_t _vpe, goff_t _addr, size_t _size, int _perms)
        : GateObject(Capability::MGATE),
          RefCounted(),
          pe(_pe),
          vpe(_vpe),
          addr(_addr),
          size(_size),
          perms(_perms),
          derived(false) {
    }
    ~MGateObject();

    peid_t pe;
    vpeid_t vpe;
    goff_t addr;
    size_t size;
    int perms;
    bool derived;
};

class SessObject : public SlabObject<SessObject>, public m3::RefCounted {
public:
    explicit SessObject(Service *_srv, word_t _ident)
        : RefCounted(),
          servowned(),
          ident(_ident),
          srv(_srv) {
    }
    ~SessObject();

    void close();

    bool servowned;
    word_t ident;
    m3::Reference<Service> srv;
};

class EPObject : public SlabObject<EPObject>, public m3::RefCounted {
public:
    explicit EPObject(vpeid_t _vpe, epid_t _ep)
        : RefCounted(),
          ep(_ep),
          vpe(_vpe),
          gate() {
    }
    ~EPObject();

    epid_t ep;
    vpeid_t vpe;
    GateObject *gate;
};

class RGateCapability : public SlabObject<RGateCapability>, public Capability {
public:
    explicit RGateCapability(CapTable *tbl, capsel_t sel, int order, int msgorder)
        : Capability(tbl, sel, RGATE),
          obj(new RGateObject(order, msgorder)) {
    }

    virtual GateObject *as_gate() override {
        return &*obj;
    }

    void printInfo(m3::OStream &os) const override;

protected:
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        RGateCapability *c = new RGateCapability(*this);
        c->put(tbl, sel);
        return c;
    }

public:
    m3::Reference<RGateObject> obj;
};

class SGateCapability : public SlabObject<SGateCapability>, public Capability {
public:
    explicit SGateCapability(CapTable *tbl, capsel_t sel, RGateObject *rgate, label_t label, word_t credits)
        : Capability(tbl, sel, SGATE),
          obj(new SGateObject(rgate, label, credits)) {
    }

    virtual GateObject *as_gate() override {
        return &*obj;
    }

    void printInfo(m3::OStream &os) const override;

protected:
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        SGateCapability *c = new SGateCapability(*this);
        c->put(tbl, sel);
        return c;
    }

public:
    m3::Reference<SGateObject> obj;
};

class MGateCapability : public SlabObject<MGateCapability>, public Capability {
public:
    explicit MGateCapability(CapTable *tbl, capsel_t sel, peid_t pe, vpeid_t vpe, goff_t addr,
                             size_t size, int perms)
        : Capability(tbl, sel, MGATE),
          obj(new MGateObject(pe, vpe, addr, size, perms)) {
    }

    virtual GateObject *as_gate() override {
        return &*obj;
    }

    void printInfo(m3::OStream &os) const override;

private:
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        MGateCapability *c = new MGateCapability(*this);
        c->put(tbl, sel);
        return c;
    }

public:
    m3::Reference<MGateObject> obj;
};

class MapCapability : public SlabObject<MapCapability>, public Capability {
public:
    explicit MapCapability(CapTable *tbl, capsel_t sel, gaddr_t _phys, uint pages, int _attr);

    void remap(gaddr_t _phys, int _attr);

    void printInfo(m3::OStream &os) const override;

private:
    virtual void revoke() override;
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        return new MapCapability(tbl, sel, phys, length, attr);
    }

public:
    gaddr_t phys;
    int attr;
};

class ServCapability : public SlabObject<ServCapability>, public Capability {
public:
    explicit ServCapability(CapTable *tbl, capsel_t sel, Service *_obj)
        : Capability(tbl, sel, SERV),
          obj(_obj) {
    }

    void printInfo(m3::OStream &os) const override;

private:
    virtual void revoke() override;
    virtual Capability *clone(CapTable *, capsel_t) override {
        /* not cloneable */
        return nullptr;
    }

public:
    m3::Reference<Service> obj;
};

class SessCapability : public SlabObject<SessCapability>, public Capability {
public:
    explicit SessCapability(CapTable *tbl, capsel_t sel, Service *srv, word_t ident)
        : Capability(tbl, sel, SESS),
          obj(new SessObject(srv, ident)) {
    }

    void printInfo(m3::OStream &os) const override;

private:
    virtual void revoke() override;
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        SessCapability *s = new SessCapability(*this);
        s->put(tbl, sel);
        return s;
    }

public:
    m3::Reference<SessObject> obj;
};

class EPCapability : public SlabObject<EPCapability>, public Capability {
public:
    explicit EPCapability(CapTable *tbl, capsel_t sel, vpeid_t vpe, epid_t ep)
        : Capability(tbl, sel, EP),
          obj(new EPObject(vpe, ep)) {
    }

    void printInfo(m3::OStream &os) const override;

private:
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        EPCapability *e = new EPCapability(*this);
        e->put(tbl, sel);
        return e;
    }

public:
    m3::Reference<EPObject> obj;
};

class VPECapability : public SlabObject<VPECapability>, public Capability {
public:
    explicit VPECapability(CapTable *tbl, capsel_t sel, VPE *p)
        : Capability(tbl, sel, VIRTPE),
          obj(p) {
    }

    void printInfo(m3::OStream &os) const override;

private:
    virtual void revoke() override;
    virtual Capability *clone(CapTable *tbl, capsel_t sel) override {
        VPECapability *v = new VPECapability(*this);
        v->put(tbl, sel);
        return v;
    }

public:
    m3::Reference<VPE> obj;
};

inline EPObject *GateObject::ep_of_vpe(vpeid_t vpe) {
    for(auto u = epuser.begin(); u != epuser.end(); ++u) {
        if(u->ep->vpe == vpe)
            return u->ep;
    }
    return nullptr;
}

inline void GateObject::print_eps(m3::OStream &os) {
    os << "[";
    for(auto u = epuser.begin(); u != epuser.end(); ) {
        os << "VPE" << u->ep->vpe << ":EP" << u->ep->ep;
        if(++u != epuser.end())
            os << ", ";
    }
    os << "]";
}

}
