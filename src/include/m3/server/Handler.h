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
#include <base/util/CapRngDesc.h>
#include <base/Errors.h>

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
    virtual void handle_open(GateIStream &args) {
        reply_vmsg_on(args, Errors::NO_ERROR, add_session(new SESS()));
    }
    virtual void handle_obtain(SESS *, RecvBuf *, GateIStream &args, uint) {
        reply_vmsg_on(args, Errors::NOT_SUP);
    }
    virtual void handle_delegate(SESS *, GateIStream &args, uint) {
        reply_vmsg_on(args, Errors::NOT_SUP);
    }
    virtual void handle_close(SESS *sess, GateIStream &args) {
        remove_session(sess);
        delete sess;
        reply_vmsg_on(args, Errors::NO_ERROR);
    }
    virtual void handle_shutdown() {
    }

private:
    SList<SESS> _sessions;
};

}
