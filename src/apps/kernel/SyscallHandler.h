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

#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/Syscalls.h>

#include "CapTable.h"
#include "Services.h"
#include "KVPE.h"

namespace m3 {

class SyscallHandler : public RequestHandler<SyscallHandler, Syscalls::Operation, Syscalls::COUNT> {
    explicit SyscallHandler();

public:
    using server_type = Server<SyscallHandler>;

    static SyscallHandler &get() {
        return _inst;
    }

    size_t chanid() const {
        // we can use it here because we won't issue syscalls ourself
        return DTU::SYSC_CHAN;
    }
    size_t srvchanid() const {
        return _srvrcvbuf.chanid();
    }
    RecvBuf *rcvbuf() {
        return &_rcvbuf;
    }
    RecvBuf *srvrcvbuf() {
        return &_srvrcvbuf;
    }

    RecvGate create_gate(KVPE *vpe) {
        using std::placeholders::_1;
        using std::placeholders::_2;
        RecvGate syscc = RecvGate::create(&_rcvbuf, vpe);
        add_session(vpe);
        return syscc;
    }

    void createsrv(RecvGate &gate, GateIStream &is);
    void createsess(RecvGate &gate, GateIStream &is);
    void creategate(RecvGate &gate, GateIStream &is);
    void createvpe(RecvGate &gate, GateIStream &is);
    void attachrb(RecvGate &gate, GateIStream &is);
    void detachrb(RecvGate &gate, GateIStream &is);
    void exchange(RecvGate &gate, GateIStream &is);
    void vpectrl(RecvGate &gate, GateIStream &is);
    void delegate(RecvGate &gate, GateIStream &is);
    void obtain(RecvGate &gate, GateIStream &is);
    void activate(RecvGate &gate, GateIStream &is);
    void reqmem(RecvGate &gate, GateIStream &is);
    void derivemem(RecvGate &gate, GateIStream &is);
    void revoke(RecvGate &gate, GateIStream &is);
    void exit(RecvGate &gate, GateIStream &is);
    void noop(RecvGate &gate, GateIStream &is);

#if defined(__host__)
    void init(m3::RecvGate &gate, m3::GateIStream &is);
#endif

private:
    Errors::Code do_exchange(KVPE *v1, KVPE *v2, const CapRngDesc &c1, const CapRngDesc &c2, bool obtain);
    void exchange_over_sess(RecvGate &gate, GateIStream &is, bool obtain);

    RecvBuf _rcvbuf;
    RecvBuf _srvrcvbuf;
    static SyscallHandler _inst;
};

}
