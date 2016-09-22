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
class RecvBuf;

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
        : _gate(ObjCap::INVALID, 0, nullptr, DTU::SYSC_EP), _rlabel(_gate.receive_gate()->label()),
          _rep(_gate.receive_gate()->epid()) {
    }

public:
    Errors::Code activate(size_t ep, capsel_t oldcap, capsel_t newcap);
    Errors::Code activatereply(size_t ep, uintptr_t msgaddr);
    Errors::Code createsrv(capsel_t srv, label_t label, const String &name);
    Errors::Code createsess(capsel_t vpe, capsel_t cap, const String &name, word_t arg);
    Errors::Code createsessat(capsel_t srv, capsel_t sess, word_t ident);
    Errors::Code creategate(capsel_t vpe, capsel_t dst, label_t label, size_t ep, word_t credits);
    Errors::Code createvpe(capsel_t vpe, capsel_t mem, const String &name, PEDesc &pe, capsel_t gate, size_t ep, bool tmuxable);
    Errors::Code createmap(capsel_t vpe, capsel_t mem, capsel_t first, capsel_t pages, capsel_t dst, int perms);
    Errors::Code attachrb(capsel_t vpe, size_t ep, uintptr_t addr, int order, int msgorder, uint flags);
    Errors::Code detachrb(capsel_t vpe, size_t ep);
    Errors::Code exchange(capsel_t vpe, const KIF::CapRngDesc &own, const KIF::CapRngDesc &other, bool obtain);
    // we need the pid only to support the VPE abstraction on the host
    Errors::Code vpectrl(capsel_t vpe, KIF::Syscall::VPEOp op, int pid, int *exitcode);
    Errors::Code delegate(capsel_t vpe, capsel_t sess, const KIF::CapRngDesc &crd,
        size_t *argcount = nullptr, word_t *args = nullptr);
    Errors::Code obtain(capsel_t vpe, capsel_t sess, const KIF::CapRngDesc &crd,
        size_t *argcount = nullptr, word_t *args = nullptr);
    Errors::Code reqmem(capsel_t cap, size_t size, int perms) {
        return reqmemat(cap, -1, size, perms);
    }
    Errors::Code reqmemat(capsel_t cap, uintptr_t addr, size_t size, int perms);
    Errors::Code derivemem(capsel_t src, capsel_t dst, size_t offset, size_t size, int perms);
    Errors::Code revoke(capsel_t vpe, const KIF::CapRngDesc &crd, bool own = true);
    void exit(int exitcode);
    Errors::Code noop();

private:
    DTU::Message *send_receive(const void *msg, size_t size);
    Errors::Code send_receive_result(const void *msg, size_t size);
    Errors::Code exchangesess(capsel_t vpe, capsel_t sess, const KIF::CapRngDesc &crd,
        size_t *argcount, word_t *args, bool obtain);

    SendGate _gate;
    label_t _rlabel;
    int _rep;
    static Syscalls _inst;
};

}
