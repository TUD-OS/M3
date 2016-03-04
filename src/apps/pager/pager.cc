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

#include <m3/col/Treap.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/service/Pager.h>
#include <m3/vfs/LocList.h>
#include <m3/GateStream.h>
#include <m3/WorkLoop.h>
#include <m3/Log.h>

#include "DataSpace.h"

using namespace m3;

static constexpr size_t MAX_VIRT_ADDR = (1UL << (DTU::LEVEL_CNT * DTU::LEVEL_BITS + PAGE_BITS)) - 1;

class MemSessionData : public RequestSessionData {
public:
    explicit MemSessionData()
        : RequestSessionData(), id(nextId++), vpe(ObjCap::INVALID), dstree() {
    }
    ~MemSessionData() {
        DataSpace *ds;
        while((ds = static_cast<DataSpace*>(dstree.remove_root())))
            delete ds;
    }

    const DataSpace *find(uintptr_t virt) const {
        return dstree.find(virt);
    }

    capsel_t init(capsel_t _vpe) {
        vpe = ObjCap(ObjCap::VIRTPE, _vpe);
        return vpe.sel();
    }

    int id;
    ObjCap vpe;
    Treap<DataSpace> dstree;
    static int nextId;
};

int MemSessionData::nextId = 1;

class MemReqHandler;
typedef RequestHandler<MemReqHandler, Pager::Operation, Pager::COUNT, MemSessionData> base_class_t;

class MemReqHandler : public base_class_t {
public:
    explicit MemReqHandler() : base_class_t() {
        add_operation(Pager::PAGEFAULT, &MemReqHandler::pf);
        add_operation(Pager::MAP_ANON, &MemReqHandler::map_anon);
        add_operation(Pager::UNMAP, &MemReqHandler::unmap);
    }

    virtual size_t credits() override {
        return Server<MemReqHandler>::DEF_MSGSIZE;
    }

    virtual void handle_delegate(MemSessionData *sess, GateIStream &args, uint capcount) override {
        if(capcount != 1) {
            reply_vmsg_on(args, Errors::INV_ARGS);
            return;
        }

        capsel_t sel;
        uintptr_t virt = 0;
        if(sess->vpe.sel() == ObjCap::INVALID)
            sel = sess->init(VPE::self().alloc_cap());
        else
            sel = map_ds(sess, args, &virt);
        reply_vmsg_on(args, Errors::NO_ERROR, CapRngDesc(CapRngDesc::OBJ, sel), virt);
    }

    void pf(RecvGate &gate, GateIStream &is) {
        MemSessionData *sess = gate.session<MemSessionData>();
        uint64_t virt, access;
        is >> virt >> access;

        // we are not interested in that flag
        access &= ~DTU::PTE_I;

        // access == PTE_GONE indicates, that the VPE that owns the memory is not available
        // TODO notify the kernel to run the VPE again or migrate it and update the PTEs

        LOG(PF, sess->id << " : mem::pf(virt=" << fmt(virt, "p")
            << ", access " << fmt(access, "#x") << ")");

        if(sess->vpe.sel() == ObjCap::INVALID) {
            LOG(PF, "Invalid session");
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }

        DataSpace *ds = sess->dstree.find(virt);
        if(!ds) {
            LOG(PF, "No dataspace attached at " << fmt(virt, "p"));
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }

        if((ds->flags & access) != access) {
            LOG(PF, "Access at " << fmt(virt, "p") << " for " << fmt(access, "#x")
                << " not allowed: " << fmt(ds->flags, "#x"));
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }

        int first;
        size_t pages;
        capsel_t mem;
        Errors::Code res = ds->get_page(&virt, &first, &pages, &mem);
        if(res != Errors::NO_ERROR) {
            LOG(PF, "Getting page failed: " << Errors::to_string(res));
            reply_vmsg(gate, res);
            return;
        }

        if(pages > 0) {
            res = Syscalls::get().createmap(sess->vpe.sel(), mem, first,
                pages, virt >> PAGE_BITS, ds->flags);
            if(res != Errors::NO_ERROR) {
                LOG(PF, "Unable to create PTEs: " << Errors::to_string(res));
                reply_vmsg(gate, res);
                return;
            }
        }

        reply_vmsg(gate, Errors::NO_ERROR);
    }

    void map_anon(RecvGate &gate, GateIStream &is) {
        MemSessionData *sess = gate.session<MemSessionData>();
        uintptr_t virt;
        size_t len;
        int prot, flags;
        is >> virt >> len >> prot >> flags;

        LOG(PF, sess->id << " : mem::map_anon(virt=" << fmt(virt, "p")
            << ", len " << fmt(len, "#x") << ", prot=" << fmt(prot, "#x")
            << ", flags=" << fmt(flags, "#x") << ")");

        virt = Math::round_dn(virt, PAGE_SIZE);
        len = Math::round_up(len, PAGE_SIZE);

        if(virt + len <= virt || virt >= MAX_VIRT_ADDR) {
            LOG(PF, "Invalid virtual address / size");
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }
        if((virt & PAGE_BITS) || (len & PAGE_BITS)) {
            LOG(PF, "Virtual address or size not properly aligned");
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }
        if(prot == 0 || (prot & ~DTU::PTE_RWX)) {
            LOG(PF, "Invalid protection flags");
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }

        // TODO determine/validate virt+len
        sess->dstree.insert(new AnonDataSpace(virt, len, prot | flags));

        reply_vmsg(gate, Errors::NO_ERROR, virt);
    }

    capsel_t map_ds(MemSessionData *sess, GateIStream &args, uintptr_t *virt) {
        size_t len, offset;
        int prot, flags, id;
        args >> *virt >> len >> prot >> flags >> id >> offset;

        LOG(PF, sess->id << " : mem::map_ds(virt=" << fmt(*virt, "p")
            << ", len " << fmt(len, "#x") << ", prot=" << fmt(prot, "#x")
            << ", flags=" << fmt(flags, "#x") << ", id=" << id
            << ", offset=" << fmt(offset, "#x") << ")");

        *virt = Math::round_dn(*virt, PAGE_SIZE);
        len = Math::round_up(len, PAGE_SIZE);

        if((*virt & PAGE_BITS) || (len & PAGE_BITS)) {
            LOG(PF, "Virtual address or size not properly aligned");
            return Errors::INV_ARGS;
        }

        // TODO determine/validate virt+len
        ExternalDataSpace *ds = new ExternalDataSpace(*virt, len, prot | flags, id, offset);
        sess->dstree.insert(ds);

        return ds->sess.sel();
    }

    void unmap(RecvGate &gate, GateIStream &is) {
        MemSessionData *sess = gate.session<MemSessionData>();
        uintptr_t virt;
        is >> virt;

        LOG(PF, sess->id << " : mem::unmap(virt=" << fmt(virt, "p") << ")");

        Errors::Code res = Errors::INV_ARGS;
        DataSpace *ds = sess->dstree.find(virt);
        if(ds) {
            sess->dstree.remove(ds);
            delete ds;
            res = Errors::NO_ERROR;
        }
        else
            LOG(PF, "No dataspace attached at " << fmt(virt, "p"));

        reply_vmsg(gate, res);
    }
};

int main() {
    Server<MemReqHandler> srv("pager", new MemReqHandler());
    WorkLoop::get().run();
    return 0;
}
