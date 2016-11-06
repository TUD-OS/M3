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
#include <base/DTU.h>

namespace kernel {

class VPE;

class SyscallHandler {
    explicit SyscallHandler();

public:
    using handler_func = void (SyscallHandler::*)(VPE *vpe, const m3::DTU::Message *msg);

    static SyscallHandler &get() {
        return _inst;
    }

    void add_operation(m3::KIF::Syscall::Operation op, handler_func func) {
        _callbacks[op] = func;
    }

    void handle_message(VPE *vpe, const m3::DTU::Message *msg);

    epid_t ep() const {
        // we can use it here because we won't issue syscalls ourself
        return m3::DTU::SYSC_SEP;
    }
    epid_t srvep() const {
        return _serv_ep;
    }

    void pagefault(VPE *vpe, const m3::DTU::Message *msg);
    void createsrv(VPE *vpe, const m3::DTU::Message *msg);
    void createsess(VPE *vpe, const m3::DTU::Message *msg);
    void createsessat(VPE *vpe, const m3::DTU::Message *msg);
    void creatergate(VPE *vpe, const m3::DTU::Message *msg);
    void createsgate(VPE *vpe, const m3::DTU::Message *msg);
    void createmgate(VPE *vpe, const m3::DTU::Message *msg);
    void createvpe(VPE *vpe, const m3::DTU::Message *msg);
    void createmap(VPE *vpe, const m3::DTU::Message *msg);
    void activate(VPE *vpe, const m3::DTU::Message *msg);
    void vpectrl(VPE *vpe, const m3::DTU::Message *msg);
    void derivemem(VPE *vpe, const m3::DTU::Message *msg);
    void exchange(VPE *vpe, const m3::DTU::Message *msg);
    void delegate(VPE *vpe, const m3::DTU::Message *msg);
    void obtain(VPE *vpe, const m3::DTU::Message *msg);
    void revoke(VPE *vpe, const m3::DTU::Message *msg);
    void forwardmsg(VPE *vpe, const m3::DTU::Message *msg);
    void forwardmem(VPE *vpe, const m3::DTU::Message *msg);
    void forwardreply(VPE *vpe, const m3::DTU::Message *msg);
    void idle(VPE *vpe, const m3::DTU::Message *msg);
    void noop(VPE *vpe, const m3::DTU::Message *msg);

private:
    void reply_msg(VPE *vpe, const m3::DTU::Message *msg, const void *reply, size_t size);
    void reply_result(VPE *vpe, const m3::DTU::Message *msg, m3::Errors::Code code);

    m3::Errors::Code wait_for(const char *name, VPE &tvpe, VPE *cur);
    m3::Errors::Code do_exchange(VPE *v1, VPE *v2, const m3::KIF::CapRngDesc &c1,
        const m3::KIF::CapRngDesc &c2, bool obtain);
    void exchange_over_sess(VPE *vpe, const m3::DTU::Message *msg, bool obtain);

    epid_t _serv_ep;
    handler_func _callbacks[m3::KIF::Syscall::COUNT];
    static SyscallHandler _inst;
};

}
