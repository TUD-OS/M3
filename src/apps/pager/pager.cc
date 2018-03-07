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
#include <base/stream/IStringStream.h>
#include <base/log/Services.h>
#include <base/CmdArgs.h>

#include <m3/com/GateStream.h>
#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>

#include "AddrSpace.h"

using namespace m3;

static constexpr int SHIFT = DTU::LEVEL_CNT * DTU::LEVEL_BITS + PAGE_BITS;
static constexpr goff_t MAX_VIRT_ADDR = (static_cast<goff_t>(1) << SHIFT) - 1;

class MemReqHandler;
typedef RequestHandler<MemReqHandler, Pager::Operation, Pager::COUNT, AddrSpace> base_class_t;

static Server<MemReqHandler> *srv;
static size_t maxAnonPages = 4;
static size_t maxExternPages = 8;

class MemReqHandler : public base_class_t {
public:
    explicit MemReqHandler() : base_class_t() {
        add_operation(Pager::PAGEFAULT, &MemReqHandler::pf);
        add_operation(Pager::CLONE, &MemReqHandler::clone);
        add_operation(Pager::MAP_ANON, &MemReqHandler::map_anon);
        add_operation(Pager::UNMAP, &MemReqHandler::unmap);
    }

    virtual Errors::Code handle_delegate(AddrSpace *sess, KIF::Service::ExchangeData &data) override {
        if(data.caps != 1 && data.caps != 2)
            return Errors::INV_ARGS;

        capsel_t sel;
        goff_t virt = 0;
        if(sess->vpe.sel() == ObjCap::INVALID)
            sel = sess->init(VPE::self().alloc_caps(2));
        else {
            if(data.args.vals[0] == Pager::DelOp::DATASPACE)
                sel = map_ds(sess, data.args.count, data.args.vals, &virt);
            else
                sel = map_mem(sess, data.args.count, data.args.vals, &virt);
            data.args.count = 1;
            data.args.vals[0] = virt;
        }

        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, sel, data.caps);
        data.caps = crd.value();
        return Errors::NONE;
    }

    virtual Errors::Code handle_obtain(AddrSpace *sess, KIF::Service::ExchangeData &data) override {
        if(!sess->send_gate())
            return base_class_t::handle_obtain(sess, data);
        if(data.caps != 1 || data.args.count != 0)
            return Errors::INV_ARGS;

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::create_clone()");

        // clone the current session and connect it to the current one
        AddrSpace *nsess = new AddrSpace(sess, VPE::self().alloc_cap());
        Syscalls::get().createsessat(nsess->sess.sel(), srv->sel(), reinterpret_cast<word_t>(nsess));
        add_session(nsess);

        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, nsess->sess.sel());
        data.caps = crd.value();
        return Errors::NONE;
    }

    void pf(GateIStream &is) {
        AddrSpace *sess = is.label<AddrSpace*>();

        uint64_t virt;
        int access;
        is >> virt >> access;

        // we are not interested in that flag
        access &= ~DTU::PTE_I;

        // access == PTE_GONE indicates, that the VPE that owns the memory is not available
        // TODO notify the kernel to run the VPE again or migrate it and update the PTEs

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::pf(virt=" << fmt(virt, "p")
            << ", access=" << fmt(access, "#x") << ")");

        // we never map page 0 and thus we tell the DTU to remember that there is no mapping
        if((virt & ~PAGE_MASK) == 0) {
            SLOG(PAGER, "No mapping at page 0");
            reply_error(is, Errors::NO_MAPPING);
            return;
        }

        if(sess->vpe.sel() == ObjCap::INVALID) {
            SLOG(PAGER, "Invalid session");
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        DataSpace *ds = sess->dstree.find(virt);
        if(!ds) {
            SLOG(PAGER, "No dataspace attached at " << fmt(virt, "p"));
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        if((ds->flags() & access) != access) {
            SLOG(PAGER, "Access at " << fmt(virt, "p") << " for " << fmt(access, "#x")
                << " not allowed: " << fmt(ds->flags(), "#x"));
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        // ask the dataspace what to do
        Errors::Code res = ds->handle_pf(virt);
        if(res != Errors::NONE) {
            SLOG(PAGER, "Unable to handle pagefault: " << Errors::to_string(res));
            reply_error(is, res);
            return;
        }

        reply_error(is, Errors::NONE);
    }

    void clone(GateIStream &is) {
        AddrSpace *sess = is.label<AddrSpace*>();

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::clone()");

        Errors::Code res = sess->clone();
        if(res != Errors::NONE)
            SLOG(PAGER, "Clone failed: " << Errors::to_string(res));

        reply_error(is, res);
    }

    void map_anon(GateIStream &is) {
        AddrSpace *sess = is.label<AddrSpace*>();
        goff_t virt;
        size_t len;
        int prot, flags;
        is >> virt >> len >> prot >> flags;

        len = Math::round_up(len + (virt & PAGE_MASK), static_cast<goff_t>(PAGE_SIZE));
        virt = Math::round_dn(virt, static_cast<goff_t>(PAGE_SIZE));

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::map_anon(virt=" << fmt(virt, "p")
            << ", len=" << fmt(len, "#x") << ", prot=" << fmt(prot, "#x")
            << ", flags=" << fmt(flags, "#x") << ")");

        if(virt + len <= virt || virt >= MAX_VIRT_ADDR) {
            SLOG(PAGER, "Invalid virtual address / size");
            reply_error(is, Errors::INV_ARGS);
            return;
        }
        if((virt & PAGE_BITS) || (len & PAGE_BITS)) {
            SLOG(PAGER, "Virtual address or size not properly aligned");
            reply_error(is, Errors::INV_ARGS);
            return;
        }
        if(prot == 0 || (prot & ~DTU::PTE_RWX)) {
            SLOG(PAGER, "Invalid protection flags");
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        // TODO determine/validate virt+len
        AnonDataSpace *ds = new AnonDataSpace(sess, maxAnonPages, virt, len, prot | flags);
        sess->add(ds);

        reply_vmsg(is, Errors::NONE, virt);
    }

    capsel_t map_mem(AddrSpace *sess, size_t argc, const xfer_t *args, goff_t *virt) {
        if(argc != 4)
            return Errors::INV_ARGS;

        *virt = args[1];
        size_t len = args[2];
        int prot = args[3];

        len = Math::round_up(len + (*virt & PAGE_MASK), static_cast<goff_t>(PAGE_SIZE));
        *virt = Math::round_dn(*virt, static_cast<goff_t>(PAGE_SIZE));

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::map_mem(virt=" << fmt(*virt, "p")
            << ", len=" << fmt(len, "#x") << ", prot=" << fmt(prot, "#x") << ")");

        if((*virt & PAGE_BITS) || (len & PAGE_BITS)) {
            SLOG(PAGER, "Virtual address or size not properly aligned");
            return Errors::INV_ARGS;
        }

        // TODO determine/validate virt+len
        AnonDataSpace *ds = new AnonDataSpace(sess, maxAnonPages, *virt, len, prot);

        // immediately insert a region, so that we don't allocate new memory on PFs
        Region *r = new Region(ds, 0, len);
        r->mem(new PhysMem(sess->mem, *virt, VPE::self().alloc_cap()));
        ds->regions().append(r);

        sess->add(ds);

        return r->mem()->gate->sel();
    }

    capsel_t map_ds(AddrSpace *sess, size_t argc, const xfer_t *args, goff_t *virt) {
        if(argc != 5)
            return Errors::INV_ARGS;

        *virt = args[1];
        size_t len = args[2];
        int flags = args[3];
        size_t offset = args[4];

        len = Math::round_up(len + (*virt & PAGE_MASK), static_cast<goff_t>(PAGE_SIZE));
        *virt = Math::round_dn(*virt, static_cast<goff_t>(PAGE_SIZE));

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::map_ds(virt=" << fmt(*virt, "p")
            << ", len=" << fmt(len, "#x") << ", flags=" << fmt(flags, "#x")
            << ", offset=" << fmt(offset, "#x") << ")");

        if((*virt & PAGE_BITS) || (len & PAGE_BITS)) {
            SLOG(PAGER, "Virtual address or size not properly aligned");
            return Errors::INV_ARGS;
        }

        // TODO determine/validate virt+len
        ExternalDataSpace *ds = new ExternalDataSpace(sess, maxExternPages, *virt, len, flags, offset);
        sess->add(ds);

        return ds->sess.sel();
    }

    void unmap(GateIStream &is) {
        AddrSpace *sess = is.label<AddrSpace*>();
        goff_t virt;
        is >> virt;

        SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::unmap(virt=" << fmt(virt, "p") << ")");

        Errors::Code res = Errors::INV_ARGS;
        DataSpace *ds = sess->dstree.find(virt);
        if(ds) {
            sess->remove(ds);
            delete ds;
            res = Errors::NONE;
        }
        else
            SLOG(PAGER, "No dataspace attached at " << fmt(virt, "p"));

        reply_error(is, res);
    }
};

static void usage(const char *name) {
    Serial::get() << "Usage: " << name << " [-a <maxAnon>] [-f <maxFile>]\n";
    Serial::get() << "  -a: the max. number of anonymous pages to map at once\n";
    Serial::get() << "  -f: the max. number of file pages to map at once\n";
    exit(1);
}

int main(int argc, char **argv) {
    int opt;
    while((opt = CmdArgs::get(argc, argv, "a:f:")) != -1) {
        switch(opt) {
            case 'a': maxAnonPages = IStringStream::read_from<size_t>(CmdArgs::arg); break;
            case 'f': maxExternPages = IStringStream::read_from<size_t>(CmdArgs::arg); break;
            default:
                usage(argv[0]);
        }
    }

    srv = new Server<MemReqHandler>("pager", new MemReqHandler());
    env()->workloop()->run();
    return 0;
}
