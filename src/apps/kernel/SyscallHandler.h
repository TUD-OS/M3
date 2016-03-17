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

#include <m3/server/Server.h>
#include <m3/Syscalls.h>

#include "CapTable.h"
#include "Services.h"
#include "VPE.h"

namespace kernel {

class SyscallHandler {
    explicit SyscallHandler();

public:
    using server_type = m3::Server<SyscallHandler>;
    using handler_func = void (SyscallHandler::*)(m3::RecvGate &gate, m3::GateIStream &is);

    static SyscallHandler &get() {
        return _inst;
    }

    void add_operation(m3::Syscalls::Operation op, handler_func func) {
        _callbacks[op] = func;
    }

    void handle_message(m3::RecvGate &gate, m3::Subscriber<m3::RecvGate&> *) {
        EVENT_TRACER_handle_message();
        m3::GateIStream msg(gate);
        m3::Syscalls::Operation op;
        msg >> op;
        if(static_cast<size_t>(op) < sizeof(_callbacks) / sizeof(_callbacks[0])) {
            (this->*_callbacks[op])(gate, msg);
            return;
        }
        reply_vmsg(gate, m3::Errors::INV_ARGS);
    }

    size_t epid() const {
        // we can use it here because we won't issue syscalls ourself
        return m3::DTU::SYSC_EP;
    }
    size_t srvepid() const {
        return _srvrcvbuf.epid();
    }
    m3::RecvBuf *rcvbuf() {
        return &_rcvbuf;
    }
    m3::RecvBuf *srvrcvbuf() {
        return &_srvrcvbuf;
    }

    m3::RecvGate create_gate(VPE *vpe) {
        return m3::RecvGate::create(&_rcvbuf, vpe);
    }

    void pagefault(m3::RecvGate &gate, m3::GateIStream &is);
    void createsrv(m3::RecvGate &gate, m3::GateIStream &is);
    void createsess(m3::RecvGate &gate, m3::GateIStream &is);
    void creategate(m3::RecvGate &gate, m3::GateIStream &is);
    void createvpe(m3::RecvGate &gate, m3::GateIStream &is);
    void createmap(m3::RecvGate &gate, m3::GateIStream &is);
    void attachrb(m3::RecvGate &gate, m3::GateIStream &is);
    void detachrb(m3::RecvGate &gate, m3::GateIStream &is);
    void exchange(m3::RecvGate &gate, m3::GateIStream &is);
    void vpectrl(m3::RecvGate &gate, m3::GateIStream &is);
    void delegate(m3::RecvGate &gate, m3::GateIStream &is);
    void obtain(m3::RecvGate &gate, m3::GateIStream &is);
    void activate(m3::RecvGate &gate, m3::GateIStream &is);
    void reqmem(m3::RecvGate &gate, m3::GateIStream &is);
    void derivemem(m3::RecvGate &gate, m3::GateIStream &is);
    void revoke(m3::RecvGate &gate, m3::GateIStream &is);
    void exit(m3::RecvGate &gate, m3::GateIStream &is);
    void noop(m3::RecvGate &gate, m3::GateIStream &is);

#if defined(__host__)
    void init(m3::RecvGate &gate, m3::GateIStream &is);
#endif

private:
    m3::Errors::Code do_exchange(VPE *v1, VPE *v2, const m3::CapRngDesc &c1, const m3::CapRngDesc &c2, bool obtain);
    void exchange_over_sess(m3::RecvGate &gate, m3::GateIStream &is, bool obtain);
    void tryTerminate();

    m3::RecvBuf _rcvbuf;
    m3::RecvBuf _srvrcvbuf;
    // +1 for init on host
    handler_func _callbacks[m3::Syscalls::COUNT + 1];
    static SyscallHandler _inst;
};

}
