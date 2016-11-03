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

#include <base/tracing/Tracing.h>
#include <base/Errors.h>

#include <m3/com/GateStream.h>
#include <m3/com/SendGate.h>
#include <m3/com/RecvGate.h>
#include <m3/server/Handler.h>

namespace m3 {

template<typename CLS, typename OP, size_t OPCNT, class SESS>
class RequestHandler;

class RequestSessionData : public SessionData {
    template<typename CLS, typename OP, size_t OPCNT, class SESS>
    friend class RequestHandler;

public:
    explicit RequestSessionData() : _rgate(), _sgate() {
    }
    ~RequestSessionData() {
        delete _rgate;
        delete _sgate;
    }

    SendGate *send_gate() {
        return _sgate;
    }
    RecvGate *recv_gate() {
        return _rgate;
    }

private:
    RecvGate *_rgate;
    SendGate *_sgate;
};

template<typename CLS, typename OP, size_t OPCNT, class SESS = RequestSessionData>
class RequestHandler : public Handler<SESS> {
    template<class HDL>
    friend class Server;

    using handler_func = void (CLS::*)(GateIStream &is);

public:
    static const int DEF_ORDER      = nextlog2<8192>::val;
    static const int DEF_MSGORDER   = nextlog2<256>::val;

    explicit RequestHandler(int order = DEF_ORDER, int msgorder = DEF_MSGORDER)
        : Handler<SESS>(), _msgorder(msgorder), _rbuf(RecvBuf::create(order, msgorder)), _callbacks() {
    }

    void add_operation(OP op, handler_func func) {
        _callbacks[op] = func;
    }

protected:
    virtual Errors::Code handle_obtain(SESS *sess, KIF::Service::ExchangeData &data) override {
        using std::placeholders::_1;
        using std::placeholders::_2;
        if(sess->send_gate() || data.argcount > 0 || data.caps != 1)
            return Errors::INV_ARGS;

        sess->_rgate = new RecvGate(RecvGate::create(&_rbuf, sess));
        sess->_sgate = new SendGate(SendGate::create(1UL << _msgorder, sess->_rgate));
        sess->_rgate->subscribe(std::bind(&RequestHandler::handle_message, this, _1, _2));

        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, sess->send_gate()->sel());
        data.caps = crd.value();
        return Errors::NO_ERROR;
    }

    virtual void handle_shutdown() override {
        _rbuf.disable();
    }

public:
    void handle_message(GateIStream &msg, Subscriber<GateIStream&> *) {
        EVENT_TRACER_Service_request();
        OP op;
        msg >> op;
        if(static_cast<size_t>(op) < sizeof(_callbacks) / sizeof(_callbacks[0])) {
            (static_cast<CLS*>(this)->*_callbacks[op])(msg);
            return;
        }

        reply_error(msg, Errors::INV_ARGS);
    }

private:
    int _msgorder;
    RecvBuf _rbuf;
    handler_func _callbacks[OPCNT];
};

}
