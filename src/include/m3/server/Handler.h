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

#include <base/col/SList.h>
#include <base/Errors.h>
#include <base/KIF.h>

#include <m3/com/GateStream.h>
#include <m3/com/RecvGate.h>
#include <m3/com/RecvBuf.h>

namespace m3 {

template<class HDL>
class Server;

class SessionData : public SListItem {
};

template<class SESS = SessionData>
class Handler {
    template<class HDL>
    friend class Server;

public:
    using session_type  = SESS;
    using iterator      = typename SList<SESS>::iterator;

    explicit Handler() : _sessions() {
    }
    virtual ~Handler() {
    }

    iterator begin() {
        return _sessions.begin();
    }
    iterator end() {
        return _sessions.end();
    }
    size_t count() {
        return _sessions.length();
    }

    virtual SESS *add_session(SESS *s) {
        _sessions.append(s);
        return s;
    }
    virtual void remove_session(SESS *s) {
        _sessions.remove(s);
    }

protected:
    virtual Errors::Code handle_open(SESS **sess, word_t) {
        *sess = new SESS();
        return Errors::NO_ERROR;
    }
    virtual Errors::Code handle_obtain(SESS *, RecvBuf *, KIF::Service::ExchangeData &) {
        return Errors::NOT_SUP;
    }
    virtual Errors::Code handle_delegate(SESS *, KIF::Service::ExchangeData &) {
        return Errors::NOT_SUP;
    }
    virtual Errors::Code handle_close(SESS *sess) {
        remove_session(sess);
        delete sess;
        return Errors::NO_ERROR;
    }
    virtual void handle_shutdown() {
    }

private:
    SList<SESS> _sessions;
};

}
