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

#include <m3/Common.h>
#include <m3/cap/RecvGate.h>
#include <m3/stream/Serial.h>
#include <m3/Env.h>
#include <m3/RecvBuf.h>
#include <m3/Log.h>

#include "../../KEPMux.h"

namespace m3 {

class BaremetalKEnvBackend : public BaremetalEnvBackend {
public:
    virtual void init() override {
        env()->coreid = KERNEL_CORE;

        def_recvbuf = new RecvBuf(RecvBuf::bindto(
            DTU::DEF_RECVEP, reinterpret_cast<void*>(DEF_RCVBUF), DEF_RCVBUF_ORDER, 0));
        def_recvgate = new RecvGate(RecvGate::create(def_recvbuf));

        Serial::init("kernel", KERNEL_CORE);
    }

    virtual void reinit() override {
        // not used
    }

    virtual void switch_ep(size_t victim, capsel_t oldcap, capsel_t newcap) override {
        KEPMux::switch_ep(victim, oldcap, newcap);
    }
};

EXTERN_C void init_env(Env *e) {
    e->backend = new BaremetalKEnvBackend();
}

}
