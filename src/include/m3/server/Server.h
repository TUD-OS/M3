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
          _epid(VPE::self().alloc_ep()),
          _rcvbuf(RecvBuf::create(_epid, buford, msgord, 0)),
          _ctrl_rgate(RecvGate::create(&_rcvbuf)),
          _ctrl_sgate(SendGate::create(DEF_MSGSIZE, &_ctrl_rgate)) {
        LLOG(SERV, "create(" << name << ")");
        Syscalls::get().createsrv(_ctrl_sgate.sel(), sel(), name);

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
            while((msg = DTU::get().fetch_msg(_ctrl_rgate.epid()))) {
                GateIStream is(_ctrl_rgate, msg, Errors::NO_ERROR);
                handle_message(is, nullptr);
            }
        }
        // don't revoke it again
        flags(ObjCap::KEEP_CAP);
        // free endpoint
        VPE::self().free_ep(_epid);
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
    void handle_message(GateIStream &msg, Subscriber<GateIStream&> *) {
        KIF::Service::Command op;
        msg >> op;
        if(static_cast<size_t>(op) < ARRAY_SIZE(_ctrl_handler)) {
            (this->*_ctrl_handler[op])(msg);
            return;
        }
        reply_vmsg(msg, Errors::INV_ARGS);
    }

    void handle_open(GateIStream &is) {
        EVENT_TRACER_Service_open();

        word_t sessptr = reinterpret_cast<word_t>(_handler->handle_open(is));

        LLOG(SERV, fmt(sessptr, "#x") << ": open()");
    }

    void handle_obtain(GateIStream &is) {
        EVENT_TRACER_Service_obtain();
        word_t sessptr;
        uint capcount;
        is >> sessptr >> capcount;

        LLOG(SERV, fmt(sessptr, "#x") << ": obtain(caps=" << capcount << ")");

        typename HDL::session_type *sess = reinterpret_cast<typename HDL::session_type*>(sessptr);
        _handler->handle_obtain(sess, &_rcvbuf, is, capcount);
    }

    void handle_delegate(GateIStream &is) {
        EVENT_TRACER_Service_delegate();
        word_t sessptr;
        uint capcount;
        is >> sessptr >> capcount;

        LLOG(SERV, fmt(sessptr, "#x") << ": delegate(caps=" << capcount << ")");

        typename HDL::session_type *sess = reinterpret_cast<typename HDL::session_type*>(sessptr);
        _handler->handle_delegate(sess, is, capcount);
    }

    void handle_close(GateIStream &is) {
        EVENT_TRACER_Service_close();
        word_t sessptr;
        is >> sessptr;

        LLOG(SERV, fmt(sessptr, "#x") << ": close()");

        typename HDL::session_type *sess = reinterpret_cast<typename HDL::session_type*>(sessptr);
        _handler->handle_close(sess, is);
    }

    void handle_shutdown(GateIStream &is) {
        EVENT_TRACER_Service_shutdown();

        LLOG(SERV, "shutdown()");

        _handler->handle_shutdown();
        shutdown();
        reply_vmsg(is, Errors::NO_ERROR);
    }

protected:
    HDL *_handler;
    handler_func _ctrl_handler[5];
    // store a copy of the endpoint-id. the RecvBuf sets it to UNBOUND on detach().
    size_t _epid;
    RecvBuf _rcvbuf;
    RecvGate _ctrl_rgate;
    SendGate _ctrl_sgate;
};

}
