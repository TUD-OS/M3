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

#include <base/log/Lib.h>
#include <base/Errors.h>
#include <base/KIF.h>

#include <m3/com/RecvGate.h>
#include <m3/server/Handler.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

namespace m3 {

template<class HDL>
class Server : public ObjCap {
    using handler_func = void (Server::*)(GateIStream &is);

public:
    static constexpr size_t DEF_BUFSIZE     = 8192;
    static constexpr size_t DEF_MSGSIZE     = 256;

    explicit Server(const String &name, HDL *handler, int buford = nextlog2<DEF_BUFSIZE>::val,
                    int msgord = nextlog2<DEF_MSGSIZE>::val)
        : ObjCap(SERVICE, VPE::self().alloc_cap()), _handler(handler), _ctrl_handler(),
          // TODO we do not always need a receive buffer for clients
          _rcvbuf(RecvBuf::create(buford, msgord)),
          _ctrl_rgate(RecvGate::create(&RecvBuf::upcall())) {
        // in this case, we activate it manually in order to create the workloop item
        _rcvbuf.activate();

        LLOG(SERV, "create(" << name << ")");
        Syscalls::get().createsrv(sel(), _ctrl_rgate.label(), name);

        using std::placeholders::_1;
        using std::placeholders::_2;
        _ctrl_rgate.subscribe(std::bind(&Server::handle_message, this, _1, _2));

        _ctrl_handler[KIF::Service::OPEN] = &Server::handle_open;
        _ctrl_handler[KIF::Service::OBTAIN] = &Server::handle_obtain;
        _ctrl_handler[KIF::Service::DELEGATE] = &Server::handle_delegate;
        _ctrl_handler[KIF::Service::CLOSE] = &Server::handle_close;
        _ctrl_handler[KIF::Service::SHUTDOWN] = &Server::handle_shutdown;
    }
    ~Server() {
        LLOG(SERV, "destroy()");
        // if it fails, there are pending requests. this might happen multiple times because
        // the kernel might have them still in the send-queue.
        KIF::CapRngDesc caps(KIF::CapRngDesc::OBJ, sel());
        while(VPE::self().revoke(caps) == Errors::MSGS_WAITING) {
            // handle all requests
            LLOG(SERV, "handling pending requests...");
            DTU::Message *msg;
            while((msg = DTU::get().fetch_msg(RecvGate::upcall().epid()))) {
                GateIStream is(RecvGate::upcall(), msg, Errors::NO_ERROR);
                handle_message(is, nullptr);
            }
        }
        // don't revoke it again
        flags(ObjCap::KEEP_CAP);
    }

    void shutdown() {
        // by disabling the receive buffer, we remove it from the WorkLoop, which in the end
        // stops the WorkLoop
        _rcvbuf.disable();
    }

    HDL &handler() {
        return *_handler;
    }

private:
    void handle_message(GateIStream &is, Subscriber<GateIStream&> *) {
        auto *req = reinterpret_cast<const KIF::DefaultRequest*>(is.message().data);
        KIF::Service::Operation op = static_cast<KIF::Service::Operation>(req->opcode);

        if(static_cast<size_t>(op) < ARRAY_SIZE(_ctrl_handler)) {
            (this->*_ctrl_handler[op])(is);
            return;
        }
        reply_error(is, Errors::INV_ARGS);
    }

    void handle_open(GateIStream &is) {
        EVENT_TRACER_Service_open();

        auto *req = reinterpret_cast<const KIF::Service::Open*>(is.message().data);

        KIF::Service::OpenReply reply;

        typename HDL::session_type *sess = nullptr;
        reply.error = _handler->handle_open(&sess, req->arg);
        if(sess) {
            _handler->add_session(sess);
            LLOG(SERV, fmt((void*)sess, "#x") << ": open()");
        }

        reply.sess = reinterpret_cast<word_t>(sess);
        is.reply(&reply, sizeof(reply));
    }

    void handle_obtain(GateIStream &is) {
        EVENT_TRACER_Service_obtain();

        auto *req = reinterpret_cast<const KIF::Service::Exchange*>(is.message().data);

        LLOG(SERV, fmt((void*)req->sess, "#x") << ": obtain(caps=" << req->data.caps << ")");

        KIF::Service::ExchangeReply reply;
        memcpy(&reply.data, &req->data, sizeof(req->data));

        typename HDL::session_type *sess = reinterpret_cast<typename HDL::session_type*>(req->sess);
        reply.error = _handler->handle_obtain(sess, &_rcvbuf, reply.data);

        is.reply(&reply, sizeof(reply));
    }

    void handle_delegate(GateIStream &is) {
        EVENT_TRACER_Service_delegate();

        auto *req = reinterpret_cast<const KIF::Service::Exchange*>(is.message().data);

        LLOG(SERV, fmt((void*)req->sess, "#x") << ": delegate(caps=" << req->data.caps << ")");

        KIF::Service::ExchangeReply reply;
        memcpy(&reply.data, &req->data, sizeof(req->data));

        typename HDL::session_type *sess = reinterpret_cast<typename HDL::session_type*>(req->sess);
        reply.error = _handler->handle_delegate(sess, reply.data);

        is.reply(&reply, sizeof(reply));
    }

    void handle_close(GateIStream &is) {
        EVENT_TRACER_Service_close();

        auto *req = reinterpret_cast<const KIF::Service::Close*>(is.message().data);

        LLOG(SERV, fmt((void*)req->sess, "#x") << ": close()");

        typename HDL::session_type *sess = reinterpret_cast<typename HDL::session_type*>(req->sess);
        Errors::Code res = _handler->handle_close(sess);

        reply_error(is, res);
    }

    void handle_shutdown(GateIStream &is) {
        EVENT_TRACER_Service_shutdown();

        LLOG(SERV, "shutdown()");

        _handler->handle_shutdown();
        shutdown();

        reply_error(is, Errors::NO_ERROR);
    }

protected:
    HDL *_handler;
    handler_func _ctrl_handler[5];
    RecvBuf _rcvbuf;
    RecvGate _ctrl_rgate;
};

}
