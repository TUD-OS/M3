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

#include <base/KIF.h>

#include "cap/CapTable.h"
#include "com/Services.h"
#include "pes/VPE.h"
#include "Gate.h"

namespace kernel {

class SyscallHandler {
    explicit SyscallHandler();

public:
    using handler_func = void (SyscallHandler::*)(RecvGate &gate, GateIStream &is);

    static SyscallHandler &get() {
        return _inst;
    }

    void add_operation(m3::KIF::Syscall::Operation op, handler_func func) {
        _callbacks[op] = func;
    }

    void handle_message(RecvGate &gate, m3::Subscriber<RecvGate&> *) {
        EVENT_TRACER_handle_message();
        GateIStream msg(gate);
        m3::KIF::Syscall::Operation op;
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
        return _serv_ep;
    }

    RecvGate create_gate(VPE *vpe) {
        return RecvGate(epid(), vpe);
    }

    void pagefault(RecvGate &gate, GateIStream &is);
    void createsrv(RecvGate &gate, GateIStream &is);
    void createsess(RecvGate &gate, GateIStream &is);
    void creategate(RecvGate &gate, GateIStream &is);
    void createvpe(RecvGate &gate, GateIStream &is);
    void createmap(RecvGate &gate, GateIStream &is);
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
    void init(RecvGate &gate, GateIStream &is);
#endif

private:
    m3::Errors::Code do_exchange(VPE *v1, VPE *v2, const m3::CapRngDesc &c1, const m3::CapRngDesc &c2, bool obtain);
    void exchange_over_sess(RecvGate &gate, GateIStream &is, bool obtain);
    void tryTerminate();

    int _serv_ep;
    // +1 for init on host
    handler_func _callbacks[m3::KIF::Syscall::COUNT + 1];
    static SyscallHandler _inst;
};

}
