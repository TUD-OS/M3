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

#include <base/col/Treap.h>
#include <base/log/Services.h>

#include <m3/com/GateStream.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/LocList.h>

#include "DataSpace.h"

using namespace m3;

static constexpr size_t MAX_VIRT_ADDR = (1UL << (DTU::LEVEL_CNT * DTU::LEVEL_BITS + PAGE_BITS)) - 1;

class MemSessionData : public RequestSessionData {
public:
    explicit MemSessionData(MemSessionData *_parent = nullptr, capsel_t _sess = ObjCap::INVALID)
       : RequestSessionData(), vpe(ObjCap::VIRTPE, ObjCap::INVALID), sess(ObjCap::SESSION, _sess),
         mem(), dstree(), parent(_parent) {
    }
    ~MemSessionData() {
        for(auto ds = dslist.begin(); ds != dslist.end(); ) {
            auto old = ds++;
            delete &*old;
        }
        delete mem;
    }

    void add(DataSpace *ds) {
        dslist.append(ds);
        dstree.insert(ds);

        // if we manipulate the address space, cloning is no longer possible
        parent = nullptr;
    }

    void remove(DataSpace *ds) {
        dslist.remove(ds);
        dstree.remove(ds);
        parent = nullptr;
    }

    Errors::Code clone() {
        if(!parent)
            return Errors::NOT_SUP;

        // TODO handle the case where we already have mappings
        for(auto ds = parent->dslist.begin(); ds != parent->dslist.end(); ++ds) {
            DataSpace *dscopy = const_cast<DataSpace*>(&*ds)->clone(mem, parent->vpe.sel());
            dslist.append(dscopy);
            dstree.insert(dscopy);
        }

        // this can be done just once
        parent = nullptr;
        return Errors::NO_ERROR;
    }

    const DataSpace *find(uintptr_t virt) const {
        return dstree.find(virt);
    }

    capsel_t init(capsel_t caps) {
        vpe = ObjCap(ObjCap::VIRTPE, caps + 0);
        mem = new MemGate(MemGate::bind(caps + 1));
        return vpe.sel();
    }

    void print(OStream &os) const {
        for(auto ds = dslist.begin(); ds != dslist.end(); ++ds)
            ds->print(os);
    }

    ObjCap vpe;
    ObjCap sess;
    MemGate *mem;
    SList<DataSpace> dslist;
    Treap<DataSpace> dstree;
    // TODO if the parent destroys his session first, we have a problem
    // atm, this basically can't happen because if the parent exits, the child is destroyed, too
    MemSessionData *parent;
    static int nextId;
};

class MemReqHandler;
typedef RequestHandler<MemReqHandler, Pager::Operation, Pager::COUNT, MemSessionData> base_class_t;

int MemSessionData::nextId = 1;

static Server<MemReqHandler> *srv;

class MemReqHandler : public base_class_t {
public:
    explicit MemReqHandler() : base_class_t() {
        add_operation(Pager::PAGEFAULT, &MemReqHandler::pf);
        add_operation(Pager::CLONE, &MemReqHandler::clone);
        add_operation(Pager::MAP_ANON, &MemReqHandler::map_anon);
        add_operation(Pager::UNMAP, &MemReqHandler::unmap);
    }

    virtual size_t credits() override {
        return Server<MemReqHandler>::DEF_MSGSIZE;
    }

    virtual void handle_delegate(MemSessionData *sess, GateIStream &args, uint capcount) override {
        if(capcount != 1 && capcount != 2) {
            reply_vmsg_on(args, Errors::INV_ARGS);
            return;
        }

        capsel_t sel;
        uintptr_t virt = 0;
        if(sess->vpe.sel() == ObjCap::INVALID)
            sel = sess->init(VPE::self().alloc_caps(2));
        else
            sel = map_ds(sess, args, &virt);
        reply_vmsg_on(args, Errors::NO_ERROR, CapRngDesc(CapRngDesc::OBJ, sel, capcount), virt);
    }

    virtual void handle_obtain(MemSessionData *sess, RecvBuf *rcvbuf, GateIStream &args, uint capcount) override {
        if(!sess->send_gate()) {
            base_class_t::handle_obtain(sess, rcvbuf, args, capcount);
            return;
        }

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::create_clone()");

        // clone the current session and connect it to the current one
        MemSessionData *nsess = new MemSessionData(sess, VPE::self().alloc_cap());
        Syscalls::get().createsessat(srv->sel(), nsess->sess.sel(), reinterpret_cast<word_t>(nsess));
        add_session(nsess);

        reply_vmsg_on(args, Errors::NO_ERROR, CapRngDesc(CapRngDesc::OBJ, nsess->sess.sel()));
    }

    void pf(GateIStream &is) {
        MemSessionData *sess = is.gate().session<MemSessionData>();
        uint64_t virt, access;
        is >> virt >> access;

        // we are not interested in that flag
        access &= ~DTU::PTE_I;

        // access == PTE_GONE indicates, that the VPE that owns the memory is not available
        // TODO notify the kernel to run the VPE again or migrate it and update the PTEs

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::pf(virt=" << fmt(virt, "p")
            << ", access " << fmt(access, "#x") << ")");

        if(sess->vpe.sel() == ObjCap::INVALID) {
            SLOG(PAGER, "Invalid session");
            reply_vmsg(is.gate(), Errors::INV_ARGS);
            return;
        }

        DataSpace *ds = sess->dstree.find(virt);
        if(!ds) {
            SLOG(PAGER, "No dataspace attached at " << fmt(virt, "p"));
            reply_vmsg(is.gate(), Errors::INV_ARGS);
            return;
        }

        if((ds->flags() & access) != access) {
            SLOG(PAGER, "Access at " << fmt(virt, "p") << " for " << fmt(access, "#x")
                << " not allowed: " << fmt(ds->flags(), "#x"));
            reply_vmsg(is.gate(), Errors::INV_ARGS);
            return;
        }

        // ask the dataspace what to do
        int first;
        size_t pages;
        capsel_t mem;
        Errors::Code res = ds->get_page(&virt, &first, &pages, &mem);
        if(res != Errors::NO_ERROR) {
            SLOG(PAGER, "Getting page failed: " << Errors::to_string(res));
            reply_vmsg(is.gate(), res);
            return;
        }

        // map the memory, if necessary
        if(pages > 0) {
            res = Syscalls::get().createmap(sess->vpe.sel(), mem, first,
                pages, virt >> PAGE_BITS, ds->flags());
            if(res != Errors::NO_ERROR) {
                SLOG(PAGER, "Unable to create PTEs: " << Errors::to_string(res));
                reply_vmsg(is.gate(), res);
                return;
            }
        }

        reply_vmsg(is.gate(), Errors::NO_ERROR);
    }

    void clone(GateIStream &is) {
        MemSessionData *sess = is.gate().session<MemSessionData>();

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::clone()");

        Errors::Code res = sess->clone();
        if(res != Errors::NO_ERROR)
            SLOG(PAGER, "Clone failed: " << Errors::to_string(res));

        reply_vmsg(is.gate(), res);
    }

    void map_anon(GateIStream &is) {
        MemSessionData *sess = is.gate().session<MemSessionData>();
        uintptr_t virt;
        size_t len;
        int prot, flags;
        is >> virt >> len >> prot >> flags;

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::map_anon(virt=" << fmt(virt, "p")
            << ", len " << fmt(len, "#x") << ", prot=" << fmt(prot, "#x")
            << ", flags=" << fmt(flags, "#x") << ")");

        virt = Math::round_dn(virt, PAGE_SIZE);
        len = Math::round_up(len, PAGE_SIZE);

        if(virt + len <= virt || virt >= MAX_VIRT_ADDR) {
            SLOG(PAGER, "Invalid virtual address / size");
            reply_vmsg(is.gate(), Errors::INV_ARGS);
            return;
        }
        if((virt & PAGE_BITS) || (len & PAGE_BITS)) {
            SLOG(PAGER, "Virtual address or size not properly aligned");
            reply_vmsg(is.gate(), Errors::INV_ARGS);
            return;
        }
        if(prot == 0 || (prot & ~DTU::PTE_RWX)) {
            SLOG(PAGER, "Invalid protection flags");
            reply_vmsg(is.gate(), Errors::INV_ARGS);
            return;
        }

        // TODO determine/validate virt+len
        AnonDataSpace *ds = new AnonDataSpace(sess->mem, virt, len, prot | flags);
        sess->add(ds);

        reply_vmsg(is.gate(), Errors::NO_ERROR, virt);
    }

    capsel_t map_ds(MemSessionData *sess, GateIStream &args, uintptr_t *virt) {
        size_t len, offset;
        int prot, flags, id;
        args >> *virt >> len >> prot >> flags >> id >> offset;

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::map_ds(virt=" << fmt(*virt, "p")
            << ", len " << fmt(len, "#x") << ", prot=" << fmt(prot, "#x")
            << ", flags=" << fmt(flags, "#x") << ", id=" << id
            << ", offset=" << fmt(offset, "#x") << ")");

        *virt = Math::round_dn(*virt, PAGE_SIZE);
        len = Math::round_up(len, PAGE_SIZE);

        if((*virt & PAGE_BITS) || (len & PAGE_BITS)) {
            SLOG(PAGER, "Virtual address or size not properly aligned");
            return Errors::INV_ARGS;
        }

        // TODO determine/validate virt+len
        ExternalDataSpace *ds = new ExternalDataSpace(sess->mem, *virt, len, prot | flags, id, offset);
        sess->add(ds);

        return ds->sess.sel();
    }

    void unmap(GateIStream &is) {
        MemSessionData *sess = is.gate().session<MemSessionData>();
        uintptr_t virt;
        is >> virt;

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::unmap(virt=" << fmt(virt, "p") << ")");

        Errors::Code res = Errors::INV_ARGS;
        DataSpace *ds = sess->dstree.find(virt);
        if(ds) {
            sess->remove(ds);
            delete ds;
            res = Errors::NO_ERROR;
        }
        else
            SLOG(PAGER, "No dataspace attached at " << fmt(virt, "p"));

        reply_vmsg(is.gate(), res);
    }
};

int main() {
    srv = new Server<MemReqHandler>("pager", new MemReqHandler());
    env()->workloop()->run();
    return 0;
}
