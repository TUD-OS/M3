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

#include <m3/com/SendGate.h>
#include <m3/com/RecvGate.h>
#include <m3/session/ServerSession.h>
#include <m3/server/RequestHandler.h>

namespace m3 {

struct SimpleSession : public ServerSession {
    explicit SimpleSession(capsel_t srv_sel)
        : ServerSession(srv_sel),
          sgate() {
    }
    ~SimpleSession() {
        delete sgate;
    }

    SendGate *sgate;
};

template<typename CLS, typename OP, size_t OPCNT, size_t MSG_SIZE = 128>
class SimpleRequestHandler : public RequestHandler<CLS, OP, OPCNT, SimpleSession> {
public:
    explicit SimpleRequestHandler()
        : RequestHandler<CLS, OP, OPCNT, SimpleSession>(),
          _rgate(RecvGate::create(nextlog2<32 * MSG_SIZE>::val, nextlog2<MSG_SIZE>::val)) {
        using std::placeholders::_1;
        _rgate.start(std::bind(&SimpleRequestHandler::handle_message, this, _1));
    }

    virtual Errors::Code open(SimpleSession **sess, capsel_t srv_sel, word_t) override {
        *sess = new SimpleSession(srv_sel);
        return Errors::NONE;
    }

    virtual Errors::Code obtain(SimpleSession *sess, KIF::Service::ExchangeData &data) override {
        if(sess->sgate || data.args.count != 0 || data.caps != 1)
            return Errors::INV_ARGS;

        label_t label = reinterpret_cast<label_t>(sess);
        sess->sgate = new SendGate(SendGate::create(&_rgate, label, MSG_SIZE));

        data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sess->sgate->sel()).value();
        return Errors::NONE;
    }

    virtual Errors::Code close(SimpleSession *sess) override {
        delete sess;
        return Errors::NONE;
    }

    virtual void shutdown() override {
        _rgate.stop();
    }

private:
    RecvGate _rgate;
};

}
