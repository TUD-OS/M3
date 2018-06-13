/**
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <base/Common.h>
#include <base/log/Services.h>

#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/session/LoadGen.h>
#include <m3/session/ServerSession.h>
#include <m3/com/MemGate.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>

using namespace m3;

static char http_req[] =
    "GET /index.html HTTP/1.0\r\n" \
    "Host: localhost\r\n" \
    "User-Agent: ApacheBench/2.3\r\n" \
    "Accept: */*\r\n" \
    "\r\n";

class LoadGenSession : public m3::ServerSession {
public:
    explicit LoadGenSession(RecvGate *rgate, capsel_t srv_sel)
       : m3::ServerSession(srv_sel),
         rem_req(),
         clisgate(SendGate::create(rgate, reinterpret_cast<label_t>(this), 64)),
         sgate(),
         mgate() {
    }
    ~LoadGenSession() {
        delete mgate;
        delete sgate;
    }

    void send_request() {
        if(rem_req > 0) {
            mgate->write(http_req, sizeof(http_req), 0);
            auto msg = create_vmsg(sizeof(http_req));
            sgate->send(msg.bytes(), msg.total(), reinterpret_cast<label_t>(this));
            rem_req--;
        }
    }

    uint rem_req;
    SendGate clisgate;
    SendGate *sgate;
    MemGate *mgate;
};

class ReqHandler;
typedef RequestHandler<ReqHandler, LoadGen::Operation, LoadGen::COUNT, LoadGenSession> base_class_t;

class ReqHandler : public base_class_t {
public:
    static constexpr size_t MSG_SIZE = 64;

    explicit ReqHandler()
        : base_class_t(),
          _rgate(RecvGate::create(nextlog2<32 * MSG_SIZE>::val, nextlog2<MSG_SIZE>::val)) {
        add_operation(LoadGen::START, &ReqHandler::start);
        add_operation(LoadGen::RESPONSE, &ReqHandler::response);

        using std::placeholders::_1;
        _rgate.start(std::bind(&ReqHandler::handle_message, this, _1));
    }

    virtual Errors::Code open(LoadGenSession **sess, capsel_t srv_sel, word_t) override {
        *sess = new LoadGenSession(&_rgate, srv_sel);
        return Errors::NONE;
    }

    virtual Errors::Code obtain(LoadGenSession *sess, KIF::Service::ExchangeData &data) override {
        if(data.caps != 1)
            return Errors::INV_ARGS;

        SLOG(LOADGEN, fmt((word_t)sess, "#x") << ": mem::get_sgate()");

        data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sess->clisgate.sel()).value();
        return Errors::NONE;
    }

    virtual Errors::Code delegate(LoadGenSession *sess, KIF::Service::ExchangeData &data) override {
        if(data.caps != 2 || data.args.count != 0 || sess->sgate)
            return Errors::INV_ARGS;

        SLOG(LOADGEN, fmt((word_t)sess, "#x") << ": mem::create_chan()");

        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, VPE::self().alloc_sels(2), 2);

        sess->sgate = new SendGate(SendGate::bind(crd.start() + 0, &_rgate));
        sess->mgate = new MemGate(MemGate::bind(crd.start() + 1));

        data.caps = crd.value();
        return Errors::NONE;
    }

    virtual Errors::Code close(LoadGenSession *sess) override {
        delete sess;
        return Errors::NONE;
    }

    virtual void shutdown() override {
        _rgate.stop();
    }

    void start(GateIStream &is) {
        LoadGenSession *sess = is.label<LoadGenSession*>();
        uint count;
        is >> count;
        sess->rem_req = count;

        SLOG(LOADGEN, fmt((word_t)sess, "#x") << ": mem::start(count=" << count << ")");

        sess->send_request();
        reply_vmsg(is, Errors::NONE);
    }

    void response(GateIStream &is) {
        LoadGenSession *sess = is.label<LoadGenSession*>();
        size_t amount;
        is >> amount;

        SLOG(LOADGEN, fmt((word_t)sess, "#x") << ": mem::response(amount=" << amount << ")");

        sess->send_request();
    }

private:
    RecvGate _rgate;
};

int main(int argc, char **argv) {
    const char *name = argc > 1 ? argv[1] : "loadgen";
    Server<ReqHandler> srv(name, new ReqHandler());
    env()->workloop()->run();
    return 0;
}
