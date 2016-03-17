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

#include <m3/cap/SendGate.h>
#include <m3/util/String.h>
#include <m3/CapRngDesc.h>
#include <m3/GateStream.h>
#include <m3/Env.h>

namespace m3 {

class Env;
class RecvBuf;

class Syscalls {
    friend class Env;

    static constexpr size_t BUFSIZE     = 1024;
    static constexpr size_t MSGSIZE     = 256;

public:
    enum Operation {
        PAGEFAULT = 0,  // sent by the DTU if the PF handler is not reachable
        CREATESRV,
        CREATESESS,
        CREATEGATE,
        CREATEVPE,
        CREATEMAP,
        ATTACHRB,
        DETACHRB,
        EXCHANGE,
        VPECTRL,
        DELEGATE,
        OBTAIN,
        ACTIVATE,
        REQMEM,
        DERIVEMEM,
        REVOKE,
        EXIT,
        NOOP,
        COUNT
    };

    enum VPECtrl {
        VCTRL_START,
        VCTRL_WAIT,
    };

    static Syscalls &get() {
        return _inst;
    }

private:
    explicit Syscalls() : _gate(ObjCap::INVALID, 0, nullptr,DTU::SYSC_EP) {
    }

public:
    Errors::Code activate(size_t ep, capsel_t oldcap, capsel_t newcap);
    Errors::Code createsrv(capsel_t gate, capsel_t srv, const String &name);
    Errors::Code createsess(capsel_t vpe, capsel_t cap, const String &name, const GateOStream &args);
    Errors::Code creategate(capsel_t vpe, capsel_t dst, label_t label, size_t ep, word_t credits);
    Errors::Code createvpe(capsel_t vpe, capsel_t mem, const String &name, const String &core, capsel_t gate, size_t ep);
    Errors::Code createmap(capsel_t vpe, capsel_t mem, capsel_t first, capsel_t pages, capsel_t dst, int perms);
    Errors::Code attachrb(capsel_t vpe, size_t ep, uintptr_t addr, int order, int msgorder, uint flags);
    Errors::Code detachrb(capsel_t vpe, size_t ep);
    Errors::Code exchange(capsel_t vpe, const CapRngDesc &own, const CapRngDesc &other, bool obtain);
    // we need the pid only to support the VPE abstraction on the host
    Errors::Code vpectrl(capsel_t vpe, VPECtrl op, int pid, int *exitcode);
    Errors::Code delegate(capsel_t vpe, capsel_t sess, const CapRngDesc &crd);
    GateIStream delegate(capsel_t vpe, capsel_t sess, const CapRngDesc &crd, const GateOStream &args);
    Errors::Code obtain(capsel_t vpe, capsel_t sess, const CapRngDesc &crd);
    GateIStream obtain(capsel_t vpe, capsel_t sess, const CapRngDesc &crd, const GateOStream &args);
    Errors::Code reqmem(capsel_t cap, size_t size, int perms) {
        return reqmemat(cap, -1, size, perms);
    }
    Errors::Code reqmemat(capsel_t cap, uintptr_t addr, size_t size, int perms);
    Errors::Code derivemem(capsel_t src, capsel_t dst, size_t offset, size_t size, int perms);
    Errors::Code revoke(const CapRngDesc &crd);
    void exit(int exitcode);
    void noop();

private:
    Errors::Code finish(GateIStream &&reply);

    SendGate _gate;
    static Syscalls _inst;
};

}
