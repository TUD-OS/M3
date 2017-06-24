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

#include <base/util/Util.h>
#include <base/DTU.h>

#include <m3/com/EPMux.h>
#include <m3/ObjCap.h>

namespace m3 {

/**
 * Gate is the base class of all gates. A gate is in general the software abstraction for DTU-based
 * communication. There are three different kinds of gates: SendGate, RecvGate and MemGate.
 * SendGate and RecvGate allow to perform message-based communication, while MemGate allows to
 * read/write from/to PE-external memory.
 *
 * Before gates can be used, they need to be activated. That is, a syscall needs to be performed to
 * let the kernel configure an endpoint for the gate. For SendGate and MemGate, this is done
 * automatically by EPMux. For RecvGate, it needs to be done manually.
 *
 * On top of Gate, GateStream provides an easy way to marshall/unmarshall data.
 */
class Gate : public ObjCap {
    friend class EPMux;

    static const epid_t NODESTROY   = static_cast<epid_t>(-2);

public:
    static const epid_t UNBOUND     = static_cast<epid_t>(-1);

protected:
    explicit Gate(uint type, capsel_t cap, unsigned capflags, epid_t ep = UNBOUND)
        : ObjCap(type, cap, capflags), _ep(ep) {
    }

public:
    Gate(Gate &&g) : ObjCap(Util::move(g)), _ep(g._ep) {
        g._ep = NODESTROY;
    }
    ~Gate() {
       EPMux::get().remove(this, flags() & KEEP_CAP);
    }

    /**
     * @return the endpoint to which this gate is currently bound (might be UNBOUND)
     */
    epid_t ep() const {
        return _ep;
    }
    /**
     * Sets the endpoint for this gate, which assumes that it is already activated on this EP.
     *
     * @param ep the endpoint id
     */
    void ep(epid_t ep) {
        _ep = ep;
    }

    /**
     * Rebinds this gate to the given capability selector. Note that this will release the so far
     * bound capability selector, depending on what has been done on object creation. So, if the
     * capability has been created, it will be released. If the selector has been allocated, it will
     * be released. If not, nothing is done.
     *
     * @param newsel the new selector (might also be ObjCap::INVALID)
     */
    void rebind(capsel_t newsel) {
        EPMux::get().switch_cap(this, newsel);
        release();
        sel(newsel);
    }

protected:
    void ensure_activated() {
        if(_ep == UNBOUND && sel() != ObjCap::INVALID)
            EPMux::get().switch_to(this);
    }

private:
    epid_t _ep;
};

}
