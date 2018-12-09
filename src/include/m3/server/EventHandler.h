/*
 * Copyright (C) 2015-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/col/SList.h>

#include <m3/server/Handler.h>
#include <m3/session/ServerSession.h>
#include <m3/com/GateStream.h>
#include <m3/VPE.h>

namespace m3 {

template<class SESS>
class EventHandler;

class EventSessionData : public ServerSession, public SListItem {
    template<class SESS>
    friend class EventHandler;

public:
    explicit EventSessionData(capsel_t srv_sel)
        : ServerSession(srv_sel),
          SListItem(),
          _sgate() {
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

template<class SESS = EventSessionData>
class EventHandler : public Handler<SESS> {
    template<class HDL>
    friend class Server;

public:
    explicit EventHandler() : Handler<SESS>(), _sessions() {
    }

    template<typename... Args>
    void broadcast(const Args &... args) {
        auto msg = create_vmsg(args...);
        for(auto &h : _sessions)
            send_msg(*static_cast<SendGate*>(h.gate()), msg.bytes(), msg.total());
    }

    SList<SESS> &sessions() {
        return _sessions;
    }

protected:
    virtual Errors::Code open(SESS **sess, capsel_t srv_sel, word_t) override {
        *sess = new SESS(srv_sel);
        _sessions.append(*sess);
        return Errors::NONE;
    }

    virtual Errors::Code delegate(SESS *sess, KIF::Service::ExchangeData &data) override {
        if(sess->gate() || data.args.count != 0 || data.caps != 1)
            return Errors::INV_ARGS;

        sess->_sgate = new SendGate(SendGate::bind(VPE::self().alloc_sel(), 0));
        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, sess->gate()->sel());
        data.caps = crd.value();
        return Errors::NONE;
    }

    virtual Errors::Code close(SESS *sess) override {
        _sessions.remove(sess);
        delete sess;
        return Errors::NONE;
    }

private:
    SList<SESS> _sessions;
};

}
