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

#include <m3/cap/SendGate.h>
#include <m3/util/String.h>
#include <m3/CapRngDesc.h>

namespace m3 {

class RecvBuf;
class GateIStream;
class GateOStream;

class Syscalls {
    static constexpr size_t BUFSIZE     = 1024;
    static constexpr size_t MSGSIZE     = 256;

public:
    enum Operation {
        CREATESRV,
        CREATESESS,
        CREATEGATE,
        CREATEVPE,
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
#if defined(__host__)
        INIT,
#endif
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
    explicit Syscalls() : _gate(Cap::INVALID, 0, nullptr, ChanMng::SYSC_CHAN) {
#if defined(__host__)
        if(!Config::get().is_kernel())
            init(DTU::get().sep_regs());
#endif
    }

public:
    Errors::Code activate(size_t chan, capsel_t oldcap, capsel_t newcap);
    Errors::Code createsrv(capsel_t gate, capsel_t srv, const String &name);
    Errors::Code createsess(capsel_t cap, const String &name, const GateOStream &args);
    Errors::Code creategate(capsel_t vpe, capsel_t dst, label_t label, size_t chan, word_t credits);
    Errors::Code createvpe(capsel_t vpe, capsel_t mem, const String &name, const String &core);
    Errors::Code attachrb(capsel_t vpe, size_t chan, uintptr_t addr, int order, int msgorder, uint flags);
    Errors::Code detachrb(capsel_t vpe, size_t chan);
    Errors::Code exchange(capsel_t vpe, const CapRngDesc &own, const CapRngDesc &other, bool obtain);
    // we need the pid only to support the VPE abstraction on the host
    Errors::Code vpectrl(capsel_t vpe, VPECtrl op, int pid, int *exitcode);
    Errors::Code delegate(capsel_t sess, const CapRngDesc &crd);
    GateIStream delegate(capsel_t sess, const CapRngDesc &crd, const GateOStream &args);
    Errors::Code obtain(capsel_t sess, const CapRngDesc &crd);
    GateIStream obtain(capsel_t sess, const CapRngDesc &crd, const GateOStream &args);
    Errors::Code reqmem(capsel_t cap, size_t size, int perms) {
        return reqmemat(cap, -1, size, perms);
    }
    Errors::Code reqmemat(capsel_t cap, uintptr_t addr, size_t size, int perms);
    Errors::Code derivemem(capsel_t src, capsel_t dst, size_t offset, size_t size, int perms);
    Errors::Code revoke(const CapRngDesc &crd);
    void exit(int exitcode);
    void noop();

#if defined(__host__)
    void init(void *sepregs);
#endif

private:
    Errors::Code finish(GateIStream &&reply);

    SendGate _gate;
    static Syscalls _inst;
};

}
