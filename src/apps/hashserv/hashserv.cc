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
#include <m3/stream/Standard.h>
#include <m3/com/GateStream.h>

using namespace m3;

enum HashOp {
};

static const uint EPID          = 2;
static const size_t ACC_MEM     = 64 * 1024;
static const uintptr_t RB_ADDR  = ACC_MEM - RECVBUF_SIZE_SPM + DEF_RCVBUF_SIZE;
static const size_t RB_SIZE     = 1024;
static const size_t BUF_SIZE    = 4096;
static const size_t CLIENTS     = 8;

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
        mem = new MemGate(acc.mem().derive(caps + 1, id * BUF_SIZE, BUF_SIZE));
        return caps;
    }

    uint id;
    SendGate *sgate;
    MemGate *mem;
};

class HashReqHandler : public RequestHandler<HashReqHandler, HashOp, 0, HashSessionData> {
public:
    explicit HashReqHandler()
        : RequestHandler(), _acc("acc", PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_HASH, ACC_MEM)) {
        Errors::Code res = Syscalls::get().attachrb(
            _acc.sel(), EPID, RB_ADDR, getnextlog2(RB_SIZE), getnextlog2(RB_SIZE / CLIENTS), 0);
        if(res != Errors::NO_ERROR)
            PANIC("Unable to attach receive buffer on accelerator");
    }

    size_t credits() override {
        return Server<HashReqHandler>::DEF_MSGSIZE;
    }

    HashSessionData *handle_open(GateIStream &args) override {
        for(uint i = 0; i < CLIENTS; ++i) {
            if(!(occupied & (1 << i))) {
                HashSessionData *sess = add_session(new HashSessionData(i));
                reply_vmsg_on(args, Errors::NO_ERROR, sess);
                return sess;
            }
        }

        reply_vmsg_on(args, Errors::NO_SPACE);
        return nullptr;
    }

    void handle_obtain(HashSessionData *sess, RecvBuf *, GateIStream &args, uint capcount) override {
        if(capcount != 2 || sess->sgate) {
            reply_vmsg_on(args, Errors::INV_ARGS);
            return;
        }

        capsel_t caps = sess->connect(_acc);
        reply_vmsg_on(args, Errors::NO_ERROR, CapRngDesc(CapRngDesc::OBJ, caps, 2));
    }

private:
    VPE _acc;
};

int main() {
    Server<HashReqHandler> srv("hash", new HashReqHandler());
    if(Errors::occurred())
        exitmsg("Unable to register service 'hash'");

    env()->workloop()->run();
    return 0;
}
