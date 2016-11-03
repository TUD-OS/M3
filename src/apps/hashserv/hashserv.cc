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
#include <m3/session/Hash.h>
#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/com/GateStream.h>

#include <hash/Hash.h>

using namespace m3;

static const uintptr_t ACC_BUFS     = 0x3000;
static const uintptr_t BUF_ADDR     = 0x2000000;
static const size_t MAX_CLIENTS     = 8;

static uint32_t occupied            = 0;

class HashSessionData : public RequestSessionData {
public:
    // unused
    explicit HashSessionData() {
    }
    explicit HashSessionData(size_t _id) : RequestSessionData(), id(_id), mem(), sgate() {
        occupied |= 1 << id;
    }
    ~HashSessionData() {
        occupied &= ~(1 << id);
        delete mem;
        delete sgate;
    }

    SendGate &sendgate() {
        return *sgate;
    }
    size_t offset() const {
        return id * hash::Accel::BUF_SIZE;
    }

    capsel_t connect(VPE &acc, RecvBuf *buf, RecvGate *rgate) {
        label_t label;
        if(acc.pe().has_virtmem()) {
            label = ACC_BUFS + offset();
            mem = new MemGate(acc.mem().derive(label, hash::Accel::BUF_SIZE, MemGate::W));
        }
        else {
            label = ACC_BUFS;
            mem = new MemGate(VPE::self().mem().derive(
                BUF_ADDR + offset(), hash::Accel::BUF_SIZE, MemGate::W));
        }
        sgate = new SendGate(SendGate::create_for(buf, label, 256, rgate));
        return mem->sel();
    }

    size_t id;
    MemGate *mem;
    SendGate *sgate;
};

class HashReqHandler : public RequestHandler<HashReqHandler, Hash::Operation, Hash::COUNT, HashSessionData> {
public:
    explicit HashReqHandler(hash::Accel *accel)
        : RequestHandler(),
          _rbuf(RecvBuf::create_for(accel->get(), getnextlog2(hash::Accel::RB_SIZE), getnextlog2(hash::Accel::RB_SIZE))),
          _rgate(RecvGate::create(&RecvBuf::def())),
          _buf(),
          _mem(accel->get().mem().derive(ACC_BUFS, hash::Accel::BUF_SIZE, MemGate::W)),
          _accel(accel) {
        if(!_accel->get().pe().has_virtmem()) {
            _buf = new MemGate(MemGate::create_global(
                hash::Accel::BUF_SIZE * MAX_CLIENTS, MemGate::RW));
            Syscalls::get().createmap(VPE::self().sel(), _buf->sel(), 0,
                hash::Accel::BUF_SIZE / PAGE_SIZE, BUF_ADDR / PAGE_SIZE, MemGate::RW);
        }
        else {
            assert(_accel->get().pager() != nullptr);
            uintptr_t virt = ACC_BUFS;
            _accel->get().pager()->map_anon(&virt, hash::Accel::BUF_SIZE * MAX_CLIENTS, Pager::Prot::RW, 0);
        }

        _accel->get().delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _rbuf.sel(), 1), hash::Accel::RBUF);
        _rbuf.activate(hash::Accel::EPID, _accel->getRBAddr());
        _accel->get().start();

        add_operation(Hash::CREATE_HASH, &HashReqHandler::create_hash);
    }
    ~HashReqHandler() {
        delete _buf;
    }

    Errors::Code handle_open(HashSessionData **sess, word_t) override {
        for(size_t id = 0; id < MAX_CLIENTS; ++id) {
            if(!(occupied & (1 << id))) {
                *sess = new HashSessionData(id);
                return Errors::NO_ERROR;
            }
        }
        return Errors::NO_SPACE;
    }

    Errors::Code handle_obtain(HashSessionData *sess, KIF::Service::ExchangeData &data) override {
        if(!sess->send_gate())
            return RequestHandler::handle_obtain(sess, data);

        if(data.caps != 1 || data.argcount != 0)
            return Errors::INV_ARGS;

        capsel_t cap = sess->connect(_accel->get(), &_rbuf, &_rgate);
        data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, cap, 1).value();
        data.argcount = 0;
        return Errors::NO_ERROR;
    }

    void create_hash(GateIStream &is) {
        HashSessionData *sess = is.gate().session<HashSessionData>();
        hash::Accel::Request req;
        is >> req;

        if(req.len > hash::Accel::BUF_SIZE) {
            reply_error(is, m3::Errors::INV_ARGS);
            return;
        }

        if(!_accel->get().pe().has_virtmem())
            _mem.write_sync(reinterpret_cast<void*>(BUF_ADDR + sess->offset()), req.len, 0);

        GateIStream areply = send_receive_msg(sess->sendgate(), &req, sizeof(req));

        AutoGateOStream reply(areply.length());
        reply.put(areply);
        reply_msg(is, reply.bytes(), reply.total());
    }

private:
    RecvBuf _rbuf;
    RecvGate _rgate;
    MemGate *_buf;
    MemGate _mem;
    hash::Accel *_accel;
};

int main() {
    hash::Accel *acc = hash::Accel::create();

    HashReqHandler *handler = new HashReqHandler(acc);
    Server<HashReqHandler> srv("hash", handler);
    if(Errors::occurred())
        exitmsg("Unable to register service 'hash'");

    env()->workloop()->run();

    delete handler;
    delete acc;
    return 0;
}
