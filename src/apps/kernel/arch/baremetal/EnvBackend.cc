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
#include <m3/stream/Serial.h>
#include <m3/Env.h>
#include <m3/RecvBuf.h>
#include <m3/Log.h>

#include "../../WorkLoop.h"

namespace kernel {

class BaremetalKEnvBackend : public m3::BaremetalEnvBackend {
public:
    explicit BaremetalKEnvBackend() {
        _workloop = new WorkLoop();
    }

    virtual void init() override {
        m3::env()->coreid = KERNEL_CORE;

        // not required here
        _def_recvbuf = nullptr;
        _def_recvgate = nullptr;

        m3::Serial::init("kernel", KERNEL_CORE);
    }

    virtual void reinit() override {
        // not used
    }

    virtual void exit(int) override {
    }
    virtual void attach_recvbuf(m3::RecvBuf *) override {
    }
    virtual void detach_recvbuf(m3::RecvBuf *) override {
    }
};

EXTERN_C void init_env(m3::Env *e) {
    e->backend = new BaremetalKEnvBackend();
}

}
