/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#pragma once

#include <m3/util/SList.h>
#include <m3/cap/Gate.h>
#include <m3/cap/VPE.h>
#include <m3/server/Handler.h>
#include <m3/tracing/Tracing.h>
#include <m3/GateStream.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>
#include <m3/Errors.h>

namespace m3 {

template<class HDL>
class Server : public Cap {
    using handler_func = void (Server::*)(RecvGate &gate, GateIStream &is);

public:
    static constexpr size_t DEF_BUFSIZE     = 8192;
    static constexpr size_t DEF_MSGSIZE     = 256;

    enum Command {
        OPEN,
        OBTAIN,
        DELEGATE,
        CLOSE,
        SHUTDOWN
    };

    explicit Server(const String &name, HDL *handler, int buford = nextlog2<DEF_BUFSIZE>::val,
                    int msgord = nextlog2<DEF_MSGSIZE>::val)
        : Cap(SERVICE, VPE::self().alloc_cap()), _handler(handler), _ctrl_handler(),
          _chanid(VPE::self().alloc_chan()),
          _rcvbuf(RecvBuf::create(_chanid, buford, msgord, 0)),
          _ctrl_rgate(RecvGate::create(&_rcvbuf)),
          _ctrl_sgate(SendGate::create(DEF_MSGSIZE, &_ctrl_rgate)) {
        Syscalls::get().createsrv(_ctrl_sgate.sel(), sel(), name);

        using std::placeholders::_1;
        using std::placeholders::_2;
        _ctrl_rgate.subscribe(std::bind(&Server::handle_message, this, _1, _2));

        _ctrl_handler[OPEN] = &Server::handle_open;
        _ctrl_handler[OBTAIN] = &Server::handle_obtain;
        _ctrl_handler[DELEGATE] = &Server::handle_delegate;
        _ctrl_handler[CLOSE] = &Server::handle_close;
        _ctrl_handler[SHUTDOWN] = &Server::handle_shutdown;
    }
    ~Server() {
        // if it fails, there are pending requests. this might happen multiple times because
        // the kernel might have them still in the send-queue.
        while(Syscalls::get().revoke(CapRngDesc(sel())) != Errors::NO_ERROR) {
            // handle all requests
            while(ChanMng::get().fetch_msg(_ctrl_rgate.chanid())) {
                handle_message(_ctrl_rgate, nullptr);
                ChanMng::get().ack_message(_ctrl_rgate.chanid());
            }
        }
        // don't revoke it again
        flags(Cap::KEEP_CAP);
        // free channel
        VPE::self().free_chan(_chanid);
    }

    void shutdown() {
        // by deactivating the receive buffer, we remove it from the WorkLoop, which in the end
        // stops the WorkLoop
        _rcvbuf.detach();
    }

    HDL &handler() {
        return *_handler;
    }

private:
    void handle_message(RecvGate &gate, Subscriber<RecvGate&> *) {
        GateIStream msg(gate);
        Command op;
        msg >> op;
        if(static_cast<size_t>(op) < ARRAY_SIZE(_ctrl_handler)) {
            (this->*_ctrl_handler[op])(gate, msg);
            return;
        }
        reply_vmsg(gate, Errors::INV_ARGS);
    }

    void handle_open(RecvGate &, GateIStream &is) {
        EVENT_TRACER_Service_open();
        _handler->handle_open(is);
    }

    void handle_obtain(RecvGate &, GateIStream &is) {
        EVENT_TRACER_Service_obtain();
        word_t sessptr;
        uint capcount;
        is >> sessptr >> capcount;

        typename HDL::session_type *sess = reinterpret_cast<typename HDL::session_type*>(sessptr);
        _handler->handle_obtain(sess, &_rcvbuf, is, capcount);
    }

    void handle_delegate(RecvGate &, GateIStream &is) {
        EVENT_TRACER_Service_delegate();
        word_t sessptr;
        uint capcount;
        is >> sessptr >> capcount;

        typename HDL::session_type *sess = reinterpret_cast<typename HDL::session_type*>(sessptr);
        _handler->handle_delegate(sess, is, capcount);
    }

    void handle_close(RecvGate &, GateIStream &is) {
        EVENT_TRACER_Service_close();
        word_t sessptr;
        is >> sessptr;

        typename HDL::session_type *sess = reinterpret_cast<typename HDL::session_type*>(sessptr);
        _handler->handle_close(sess, is);
    }

    void handle_shutdown(RecvGate &, GateIStream &is) {
        EVENT_TRACER_Service_shutdown();
        _handler->handle_shutdown();
        shutdown();
        reply_vmsg_on(is, Errors::NO_ERROR);
    }

protected:
    HDL *_handler;
    handler_func _ctrl_handler[5];
    // store a copy of the channel-id. the RecvBuf sets it to UNBOUND on detach().
    size_t _chanid;
    RecvBuf _rcvbuf;
    RecvGate _ctrl_rgate;
    SendGate _ctrl_sgate;
};

}
