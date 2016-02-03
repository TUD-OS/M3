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
#include <m3/service/Memory.h>
#include <m3/GateStream.h>
#include <m3/WorkLoop.h>
#include <m3/Log.h>

using namespace m3;

static constexpr size_t MAX_VIRT_ADDR = (1UL << (DTU::LEVEL_CNT * DTU::LEVEL_BITS + PAGE_BITS)) - 1;

class Region : public TreapNode<uintptr_t> {
public:
    explicit Region(uintptr_t addr, size_t size, uint _flags)
        : TreapNode<uintptr_t>(addr), flags(_flags), _size(size) {
    }
    ~Region() {
        // revoke mappings
        CapRngDesc(CapRngDesc::MAP, addr() >> PAGE_BITS, size() >> PAGE_BITS).revoke();
    }

    bool matches(uintptr_t k) override {
        return k >= addr() && k < addr() + _size;
    }

    uintptr_t addr() const {
        return key();
    }
    size_t size() const {
        return _size;
    }

    void print(OStream &os) const override {
        os << "Region[addr=" << fmt(addr(), "p") << ", size=" << fmt(size(), "#x")
           << ", flags=" << flags << "]";
    }

    uint flags;
private:
    size_t _size;
};

class MemSessionData : public RequestSessionData {
public:
    explicit MemSessionData()
        : RequestSessionData(), id(nextId++), vpe(ObjCap::INVALID), regs(), mem(), nextPage(), pages() {
    }
    ~MemSessionData() {
        Region *reg;
        while((reg = static_cast<Region*>(regs.remove_root())))
            delete reg;
        delete mem;
    }

    const Region *find(uintptr_t virt) const {
        return regs.find(virt);
    }

    void init(capsel_t _vpe) {
        vpe = ObjCap(ObjCap::VIRTPE, _vpe);
        // TODO come up with something real ;)
        nextPage = 0;
        pages = 4;
        mem = new MemGate(MemGate::create_global(PAGE_SIZE * pages, MemGate::RWX));
    }

    Errors::Code allocPage(int *pageNo) {
        if(nextPage >= pages)
            return Errors::NO_SPACE;
        *pageNo = nextPage++;
        return Errors::NO_ERROR;
    }

    int id;
    ObjCap vpe;
    Treap<Region> regs;
    MemGate *mem;
    int nextPage;
    int pages;
    static int nextId;
};

int MemSessionData::nextId = 1;

class MemReqHandler;
typedef RequestHandler<MemReqHandler, Memory::Operation, Memory::COUNT, MemSessionData> base_class_t;

class MemReqHandler : public base_class_t {
public:
    explicit MemReqHandler() : base_class_t() {
        add_operation(Memory::PAGEFAULT, &MemReqHandler::pf);
        add_operation(Memory::MAP, &MemReqHandler::map);
        add_operation(Memory::UNMAP, &MemReqHandler::unmap);
    }

    virtual size_t credits() override {
        return Server<MemReqHandler>::DEF_MSGSIZE;
    }

    virtual void handle_delegate(MemSessionData *sess, GateIStream &args, uint capcount) override {
        if(capcount != 1 || sess->vpe.sel() != ObjCap::INVALID) {
            reply_vmsg_on(args, Errors::INV_ARGS);
            return;
        }

        sess->init(VPE::self().alloc_cap());
        reply_vmsg_on(args, Errors::NO_ERROR, CapRngDesc(CapRngDesc::OBJ, sess->vpe.sel()));
    }

    void pf(RecvGate &gate, GateIStream &is) {
        MemSessionData *sess = gate.session<MemSessionData>();
        uint64_t virt, access;
        is >> virt >> access;

        // access == PTE_GONE indicates, that the VPE that owns the memory is not available
        // TODO notify the kernel to run the VPE again or migrate it and update the PTEs

        LOG(MEM, sess->id << " : mem::pf(virt=" << fmt(virt, "p")
            << ", access " << fmt(access, "#x") << ")");

        if(sess->vpe.sel() == ObjCap::INVALID) {
            LOG(MEM, "Invalid session");
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }

        Region *reg = sess->regs.find(virt);
        if(!reg) {
            LOG(MEM, "No region attached at " << fmt(virt, "p"));
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }

        int first;
        Errors::Code res = sess->allocPage(&first);
        if(res != Errors::NO_ERROR) {
            LOG(MEM, "Not enough memory");
            reply_vmsg(gate, res);
            return;
        }

        res = Syscalls::get().createmap(sess->vpe.sel(), sess->mem->sel(), first,
            1, virt >> PAGE_BITS, MemGate::RW);
        if(res != Errors::NO_ERROR) {
            LOG(MEM, "Unable to create PTEs: " << Errors::to_string(res));
            reply_vmsg(gate, res);
            return;
        }

        reply_vmsg(gate, Errors::NO_ERROR);
    }

    void map(RecvGate &gate, GateIStream &is) {
        MemSessionData *sess = gate.session<MemSessionData>();
        uintptr_t virt;
        size_t len;
        int prot, flags;
        is >> virt >> len >> prot >> flags;

        LOG(MEM, sess->id << " : mem::map(virt=" << fmt(virt, "p")
            << ", len " << fmt(len, "#x") << ", prot=" << fmt(prot, "#x")
            << ", flags=" << fmt(flags, "#x") << ")");

        virt = Math::round_dn(virt, PAGE_SIZE);
        len = Math::round_up(len, PAGE_SIZE);

        if(virt + len <= virt || virt >= MAX_VIRT_ADDR) {
            LOG(MEM, "Invalid virtual address / size");
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }
        if(prot == 0 || (prot & ~DTU::PTE_RWX)) {
            LOG(MEM, "Invalid protection flags");
            reply_vmsg(gate, Errors::INV_ARGS);
            return;
        }

        // TODO determine/validate virt+len
        Region *reg = new Region(virt, len, prot | flags);
        sess->regs.insert(reg);

        reply_vmsg(gate, Errors::NO_ERROR, virt);
    }

    void unmap(RecvGate &gate, GateIStream &is) {
        MemSessionData *sess = gate.session<MemSessionData>();
        uintptr_t virt;
        is >> virt;

        LOG(MEM, sess->id << " : mem::unmap(virt=" << fmt(virt, "p") << ")");

        Errors::Code res = Errors::INV_ARGS;
        Region *reg = sess->regs.find(virt);
        if(reg) {
            sess->regs.remove(reg);
            delete reg;
            res = Errors::NO_ERROR;
        }
        else
            LOG(MEM, "No region attached at " << fmt(virt, "p"));

        reply_vmsg(gate, res);
    }
};

int main() {
    Server<MemReqHandler> srv("memsrv", new MemReqHandler());
    WorkLoop::get().run();
    return 0;
}
