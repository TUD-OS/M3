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
    SyscallHandler() = delete;

    using handler_func = void (*)(VPE *vpe, const m3::DTU::Message *msg);

public:
    static void init();

    static epid_t ep() {
        // we can use it here because we won't issue syscalls ourself
        return m3::DTU::SYSC_SEP;
    }
    static epid_t srvep() {
        return m3::DTU::SYSC_REP;
    }

    static void handle_message(VPE *vpe, const m3::DTU::Message *msg);

private:
    static void pagefault(VPE *vpe, const m3::DTU::Message *msg);
    static void createsrv(VPE *vpe, const m3::DTU::Message *msg);
    static void createsess(VPE *vpe, const m3::DTU::Message *msg);
    static void createsessat(VPE *vpe, const m3::DTU::Message *msg);
    static void creatergate(VPE *vpe, const m3::DTU::Message *msg);
    static void createsgate(VPE *vpe, const m3::DTU::Message *msg);
    static void createmgate(VPE *vpe, const m3::DTU::Message *msg);
    static void createvpe(VPE *vpe, const m3::DTU::Message *msg);
    static void createmap(VPE *vpe, const m3::DTU::Message *msg);
    static void activate(VPE *vpe, const m3::DTU::Message *msg);
    static void vpectrl(VPE *vpe, const m3::DTU::Message *msg);
    static void vpewait(VPE *vpe, const m3::DTU::Message *msg);
    static void derivemem(VPE *vpe, const m3::DTU::Message *msg);
    static void exchange(VPE *vpe, const m3::DTU::Message *msg);
    static void delegate(VPE *vpe, const m3::DTU::Message *msg);
    static void obtain(VPE *vpe, const m3::DTU::Message *msg);
    static void revoke(VPE *vpe, const m3::DTU::Message *msg);
    static void forwardmsg(VPE *vpe, const m3::DTU::Message *msg);
    static void forwardmem(VPE *vpe, const m3::DTU::Message *msg);
    static void forwardreply(VPE *vpe, const m3::DTU::Message *msg);
    static void noop(VPE *vpe, const m3::DTU::Message *msg);

    static void add_operation(m3::KIF::Syscall::Operation op, handler_func func) {
        _callbacks[op] = func;
    }

    static void reply_msg(VPE *vpe, const m3::DTU::Message *msg, const void *reply, size_t size);
    static void reply_result(VPE *vpe, const m3::DTU::Message *msg, m3::Errors::Code code);

    static m3::Errors::Code wait_for(const char *name, VPE &tvpe, VPE *cur, bool need_app);
    static m3::Errors::Code do_exchange(VPE *v1, VPE *v2, const m3::KIF::CapRngDesc &c1,
                                        const m3::KIF::CapRngDesc &c2, bool obtain);
    static void exchange_over_sess(VPE *vpe, const m3::DTU::Message *msg, bool obtain);

    static handler_func _callbacks[];
};

}
