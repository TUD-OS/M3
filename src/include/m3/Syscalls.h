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

#include <base/util/String.h>
#include <base/Env.h>
#include <base/KIF.h>
#include <base/PEDesc.h>

#include <m3/com/SendGate.h>
#include <m3/com/GateStream.h>

namespace m3 {

class Env;
class RecvGate;

class Syscalls {
    friend class Env;

    static constexpr size_t BUFSIZE     = 1024;
    static constexpr size_t MSGSIZE     = 256;

public:
    static Syscalls &get() {
        return _inst;
    }

private:
    explicit Syscalls()
        : _gate(ObjCap::INVALID, 0, &RecvGate::syscall(), DTU::SYSC_SEP) {
    }

public:
    Errors::Code createsrv(capsel_t srv, capsel_t rgate, const String &name);
    Errors::Code createsess(capsel_t cap, const String &name, word_t arg);
    Errors::Code createsessat(capsel_t srv, capsel_t sess, word_t ident);
    Errors::Code creatergate(capsel_t rgate, int order, int msgorder);
    Errors::Code createsgate(capsel_t rgate, capsel_t dst, label_t label, word_t credits);
    Errors::Code createmgate(capsel_t cap, size_t size, int perms) {
        return createmgateat(cap, -1, size, perms);
    }
    Errors::Code createmgateat(capsel_t cap, uintptr_t addr, size_t size, int perms);
    Errors::Code createvpe(capsel_t vpe, capsel_t mem, const String &name, PEDesc &pe, capsel_t gate, epid_t ep, bool tmuxable);
    Errors::Code createmap(capsel_t vpe, capsel_t mem, capsel_t first, capsel_t pages, capsel_t dst, int perms);

    Errors::Code activate(capsel_t vpe, epid_t ep, capsel_t cap, uintptr_t addr);
    Errors::Code vpectrl(capsel_t vpe, KIF::Syscall::VPEOp op, word_t *arg);
    Errors::Code derivemem(capsel_t src, capsel_t dst, size_t offset, size_t size, int perms);

    Errors::Code delegate(capsel_t sess, const KIF::CapRngDesc &crd, size_t *argcount = nullptr, word_t *args = nullptr);
    Errors::Code obtain(capsel_t sess, const KIF::CapRngDesc &crd, size_t *argcount = nullptr, word_t *args = nullptr);
    Errors::Code exchange(capsel_t vpe, const KIF::CapRngDesc &own, capsel_t other, bool obtain);
    Errors::Code revoke(capsel_t vpe, const KIF::CapRngDesc &crd, bool own = true);

    Errors::Code forwardmsg(capsel_t cap, const void *msg, size_t len, epid_t rep, label_t rlabel, void *event);
    Errors::Code forwardmem(capsel_t cap, void *data, size_t len, size_t offset, uint flags, void *event);
    Errors::Code forwardreply(capsel_t cap, const void *msg, size_t len, uintptr_t msgaddr, void *event);

    Errors::Code noop();

    void exit(int exitcode);

private:
    DTU::Message *send_receive(const void *msg, size_t size);
    Errors::Code send_receive_result(const void *msg, size_t size);
    Errors::Code exchangesess(capsel_t sess, const KIF::CapRngDesc &crd, size_t *argcount, word_t *args, bool obtain);

    SendGate _gate;
    static Syscalls _inst;
};

}
