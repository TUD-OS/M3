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

#include <m3/cap/ObjCap.h>
#include <m3/util/Util.h>
#include <m3/DTU.h>
#include <m3/EPMux.h>
#include <m3/Subscriber.h>
#include <m3/RecvBuf.h>

namespace m3 {

/**
 * Gate is the base-class of all gates. A Gate is in general a connection to an endpoint on a core.
 * This can be used for send/reply or for memory operations like read and write.
 *
 * On top of Gate, GateStream provides an easy way to marshall/unmarshall data.
 */
class Gate : public ObjCap, public SListItem {
    friend class EPMux;

    static const size_t NODESTROY   = -2;

public:
    static const size_t UNBOUND     = RecvBuf::UNBOUND;

    enum Operation {
        READ    = 0x0,
        WRITE   = 0x1,
        CMPXCHG = 0x2,
        SEND    = 0x3,
    };

protected:
    /**
     * Binds this gate for sending to the given capability. That is, the capability should be a
     * capability you've received from somebody else.
     */
    explicit Gate(uint type, capsel_t cap, unsigned capflags, size_t epid = UNBOUND)
        : ObjCap(type, cap, capflags), SListItem(), _epid(epid) {
    }

public:
    Gate(Gate &&g) : ObjCap(Util::move(g)), SListItem(Util::move(g)), _epid(g._epid) {
        g._epid = NODESTROY;
    }
    ~Gate() {
       EPMux::get().remove(this, flags() & KEEP_CAP);
    }

    /**
     * @return the endpoint to which this gate is currently bound (might be UNBOUND)
     */
    size_t epid() const {
        return _epid;
    }
    /**
     * @return the label for this gate
     */
    label_t label() const {
        return reinterpret_cast<label_t>(this);
    }

    /**
     * Rebinds this gate to the given capability-selector. Note that this will release the so far
     * bound capability-selector, depending on what has been done on the object-creation. So, if the
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
        if(_epid == UNBOUND && sel() != ObjCap::INVALID)
            EPMux::get().switch_to(this);
    }
    void wait_until_sent() {
        DTU::get().wait_until_ready(_epid);
    }

    Errors::Code async_cmd(Operation op, void *data, size_t datalen, size_t off, size_t size,
            label_t reply_lbl = 0, int reply_ep = 0);

private:
    size_t _epid;
};

}
