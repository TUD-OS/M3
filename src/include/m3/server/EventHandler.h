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

#include <m3/cap/VPE.h>
#include <m3/server/Handler.h>
#include <m3/GateStream.h>

namespace m3 {

class EventHandler;

class EventSessionData : public SessionData {
    friend class EventHandler;

public:
    explicit EventSessionData() : _sgate() {
    }
    ~EventSessionData() {
        delete _sgate;
    }

    SendGate *gate() {
        return _sgate;
    }

protected:
    SendGate *_sgate;
};

class EventHandler : public Handler<EventSessionData> {
    template<class HDL>
    friend class Server;

public:
    template<typename... Args>
    void broadcast(const Args &... args) {
        auto msg = create_vmsg(args...);
        for(auto &h : *this)
            msg.send(*static_cast<SendGate*>(h.gate()));
    }

protected:
    virtual void handle_delegate(EventSessionData *sess, GateIStream &args, uint capcount) override {
        // TODO like in RequestHandler, don't check additional argument size
        if(sess->gate() || /*args.remaining() > 0 || */capcount != 1) {
            reply_vmsg_on(args, Errors::INV_ARGS);
            return;
        }

        sess->_sgate = new SendGate(SendGate::bind(VPE::self().alloc_cap(), 0));
        reply_vmsg_on(args, Errors::NO_ERROR, CapRngDesc(sess->gate()->sel()));
    }

private:
    virtual size_t credits() {
        return SendGate::UNLIMITED;
    }
};

}
