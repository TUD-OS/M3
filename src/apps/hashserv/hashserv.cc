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

#include <base/log/Services.h>
#include <base/stream/IStringStream.h>
#include <base/Panic.h>

#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/com/GateStream.h>

#include <hash/Hash.h>

using namespace m3;

static const uintptr_t BUF_ADDR     = 0x2000000;
static const size_t MAX_CLIENTS     = 8;

static uint32_t occupied            = 0;

class HashSessionData : public RequestSessionData {
public:
    // unused
    explicit HashSessionData() {
    }
    explicit HashSessionData(size_t _id) : RequestSessionData(), id(_id), mem(), _hash() {
        occupied |= static_cast<uint32_t>(1) << id;
    }
    ~HashSessionData() {
        occupied &= ~(static_cast<uint32_t>(1) << id);
        delete mem;
    }

    size_t offset() const {
        return id * hash::Accel::BUF_SIZE;
    }

    capsel_t connect() {
        label_t label;
        // with VM, the client can directly access the accelerator
        if(_hash.accel()->vpe().pe().has_virtmem()) {
            label = hash::Accel::BUF_ADDR + offset();
            mem = new MemGate(_hash.accel()->vpe().mem().derive(label, hash::Accel::BUF_SIZE, MemGate::W));
        }
        // otherwise, let him copy to our address space and we copy it over
        else {
            label = hash::Accel::BUF_ADDR;
            mem = new MemGate(VPE::self().mem().derive(
                BUF_ADDR + offset(), hash::Accel::BUF_SIZE, MemGate::W));
        }
        return mem->sel();
    }

    size_t id;
    MemGate *mem;
    hash::Hash _hash;
};

class HashReqHandler : public RequestHandler<HashReqHandler, hash::Accel::Command, 3, HashSessionData> {
public:
    explicit HashReqHandler() : RequestHandler(), _buf() {
        // TODO note that we do that without pager here, because we cannot run init twice that easily
        // to start this service with a pager.

        // map the buffer, in case we need it
        _buf = new MemGate(MemGate::create_global(hash::Accel::BUF_SIZE * MAX_CLIENTS, MemGate::RW));
        Syscalls::get().createmap(BUF_ADDR / PAGE_SIZE, VPE::self().sel(),
            _buf->sel(), 0, (hash::Accel::BUF_SIZE * MAX_CLIENTS) / PAGE_SIZE, MemGate::RW);

        add_operation(hash::Accel::Command::INIT, &HashReqHandler::init);
        add_operation(hash::Accel::Command::UPDATE, &HashReqHandler::update);
        add_operation(hash::Accel::Command::FINISH, &HashReqHandler::finish);
    }

    Errors::Code handle_open(HashSessionData **sess, word_t) override {
        for(size_t id = 0; id < MAX_CLIENTS; ++id) {
            if(!(occupied & (static_cast<uint32_t>(1) << id))) {
                *sess = new HashSessionData(id);
                return Errors::NONE;
            }
        }
        return Errors::NO_SPACE;
    }

    Errors::Code handle_obtain(HashSessionData *sess, KIF::Service::ExchangeData &data) override {
        if(!sess->send_gate())
            return RequestHandler::handle_obtain(sess, data);

        if(data.caps != 1 || data.argcount != 0)
            return Errors::INV_ARGS;

        capsel_t cap = sess->connect();
        data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, cap, 1).value();
        data.argcount = 0;
        return Errors::NONE;
    }

    void init(GateIStream &is) {
        HashSessionData *sess = is.label<HashSessionData*>();
        const hash::Accel::Request *req = reinterpret_cast<const hash::Accel::Request*>(is.buffer());

        SLOG(HASH, "init(" << req->arg1 << ")");

        bool res = sess->_hash.start(static_cast<hash::Accel::Algorithm>(req->arg1));
        reply_vmsg(is, (uint64_t)res);
    }

    void update(GateIStream &is) {
        HashSessionData *sess = is.label<HashSessionData*>();
        const hash::Accel::Request *req = reinterpret_cast<const hash::Accel::Request*>(is.buffer());

        SLOG(HASH, "update(" << req->arg1 << ", " << req->arg2 << ")");

        if(req->arg1 > hash::Accel::BUF_SIZE) {
            reply_error(is, m3::Errors::INV_ARGS);
            return;
        }

        // if the client writes directly to the VM of the accelerator, don't write again here
        bool write = !sess->_hash.accel()->vpe().pe().has_virtmem();
        bool res = sess->_hash.update(reinterpret_cast<void*>(BUF_ADDR + sess->offset()),
            req->arg1, write);
        reply_vmsg(is, (uint64_t)res);
    }

    void finish(GateIStream &is) {
        HashSessionData *sess = is.label<HashSessionData*>();

        SLOG(HASH, "finish()");

        char buf[64];
        size_t len = sess->_hash.finish(buf, sizeof(buf));

        reply_vmsg(is, String(buf, len));
    }

private:
    MemGate *_buf;
};

int main() {
    HashReqHandler *handler = new HashReqHandler();
    Server<HashReqHandler> srv("hash", handler);
    if(Errors::occurred())
        exitmsg("Unable to register service 'hash'");

    env()->workloop()->run();

    delete handler;
    return 0;
}
