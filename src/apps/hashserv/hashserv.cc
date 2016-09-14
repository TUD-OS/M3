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

#include <base/stream/IStringStream.h>
#include <base/Panic.h>

#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/com/GateStream.h>

using namespace m3;

enum HashOp {
};

static const uint EPID          = 3;
static const size_t ACC_MEM     = 64 * 1024;
static const size_t RB_SIZE     = 1024;
static const size_t BUF_SIZE    = 4096;
static const size_t REPLY_SIZE  = 64;
static const size_t CLIENTS     = 8;
static const size_t BASE_ADDR   = PAGE_SIZE;
static const size_t MEM_SIZE    = Math::round_up(CLIENTS * BUF_SIZE + REPLY_SIZE, PAGE_SIZE);

static uint occupied = 0;

class HashSessionData : public RequestSessionData {
public:
    explicit HashSessionData() : RequestSessionData(), id(), sgate(), mem() {
        // UNUSED
    }

    explicit HashSessionData(uint _id) : RequestSessionData(), id(_id), sgate(), mem() {
        occupied |= 1 << id;
    }
    ~HashSessionData() {
        occupied &= ~(1 << id);
        if(sgate) {
            VPE::self().free_caps(sgate->sel(), 2);
            delete sgate;
            delete mem;
        }
    }

    capsel_t connect(VPE &acc) {
        capsel_t caps = VPE::self().alloc_caps(2);
        sgate = new SendGate(
            SendGate::create_for(acc, EPID, id, RB_SIZE / CLIENTS, nullptr, caps + 0));
        mem = new MemGate(acc.mem().derive(caps + 1, BASE_ADDR + id * BUF_SIZE, BUF_SIZE));
        return caps;
    }

    uint id;
    SendGate *sgate;
    MemGate *mem;
};

class HashAccel {
public:
    virtual ~HashAccel() {
    }

    virtual VPE &get() = 0;
    virtual uintptr_t getRBAddr() = 0;
};

class HashAccelIMem : public HashAccel {
public:
    explicit HashAccelIMem()
        : _vpe("acc", PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_HASH)) {
    }

    VPE &get() override {
        return _vpe;
    }
    uintptr_t getRBAddr() override {
        // TODO use _vpe.pe().mem_size() instead of ACC_MEM
        return ACC_MEM - RECVBUF_SIZE_SPM + DEF_RCVBUF_SIZE;
    }

private:
    VPE _vpe;
};

class HashAccelEMem : public HashAccel {
public:
    explicit HashAccelEMem()
        : _vpe("acc", PEDesc(PEType::COMP_EMEM, PEISA::ACCEL_HASH), "pager") {
        if(_vpe.pager()) {
            uintptr_t virt = BASE_ADDR;
            _vpe.pager()->map_anon(&virt, MEM_SIZE, Pager::Prot::READ | Pager::Prot::WRITE, 0);
        }
    }

    VPE &get() override {
        return _vpe;
    }
    uintptr_t getRBAddr() override {
        return RECVBUF_SPACE + DEF_RCVBUF_SIZE;
    }

private:
    VPE _vpe;
};

class HashReqHandler : public RequestHandler<HashReqHandler, HashOp, 0, HashSessionData> {
public:
    explicit HashReqHandler(VPE &vpe, uintptr_t rbBuf)
        : RequestHandler(), _acc(vpe) {
        _acc.start();

        Errors::Code res = Syscalls::get().attachrb(
            _acc.sel(), EPID, rbBuf, getnextlog2(RB_SIZE), getnextlog2(RB_SIZE / CLIENTS), 0);
        if(res != Errors::NO_ERROR)
            PANIC("Unable to attach receive buffer on accelerator");
    }

    size_t credits() override {
        return Server<HashReqHandler>::DEF_MSGSIZE;
    }

    Errors::Code handle_open(HashSessionData **sess, word_t) override {
        for(uint i = 0; i < CLIENTS; ++i) {
            if(!(occupied & (1 << i))) {
                *sess = new HashSessionData(i);
                return Errors::NO_ERROR;
            }
        }
        return Errors::NO_SPACE;
    }

    Errors::Code handle_obtain(HashSessionData *sess, RecvBuf *, KIF::Service::ExchangeData &data) override {
        if(data.caps != 2 || data.argcount != 0 || sess->sgate)
            return Errors::INV_ARGS;

        capsel_t caps = sess->connect(_acc);
        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, caps, 2);
        data.caps = crd.value();
        data.argcount = 0;
        return Errors::NO_ERROR;
    }

private:
    VPE &_acc;
};

int main() {
    HashAccel *acc = new HashAccelIMem();
    if(Errors::last != Errors::NO_ERROR) {
        delete acc;
        acc = new HashAccelEMem();
        if(Errors::last != Errors::NO_ERROR)
            exitmsg("Unable to find hash accelerator");
    }

    HashReqHandler *handler = new HashReqHandler(acc->get(), acc->getRBAddr());
    Server<HashReqHandler> srv("hash", handler);
    if(Errors::occurred())
        exitmsg("Unable to register service 'hash'");

    env()->workloop()->run();
    delete handler;
    delete acc;
    return 0;
}
