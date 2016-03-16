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
#include <m3/cap/VPE.h>
#include <m3/stream/Serial.h>
#include <m3/Env.h>
#include <m3/RecvBuf.h>

namespace m3 {

class EnvUserBackend : public BaremetalEnvBackend {
public:
    virtual void init() override {
        // wait until the kernel has initialized our core
        volatile Env *senv = env();
        while(senv->coreid == 0)
            ;

        def_recvbuf = new RecvBuf(RecvBuf::bindto(
            DTU::DEF_RECVEP, reinterpret_cast<void*>(DEF_RCVBUF), DEF_RCVBUF_ORDER, 0));
        def_recvgate = new RecvGate(RecvGate::create(def_recvbuf));

        // TODO argv is always present, isn't it?
        Serial::init(env()->argv ? env()->argv[0] : "Unknown", env()->coreid);
    }

    virtual void reinit() override {
        // wait until the kernel has initialized our core
        volatile Env *senv = env();
        while(senv->coreid == 0)
            ;

#if defined(__t3__)
        // set default receive buffer again
        DTU::get().configure_recv(def_recvbuf->epid(), reinterpret_cast<word_t>(def_recvbuf->addr()),
            def_recvbuf->order(), def_recvbuf->msgorder(), def_recvbuf->flags());
#endif

        Serial::init(env()->argv ? env()->argv[0] : "Unknown", senv->coreid);
        EPMux::get().reset();
#if defined(__t2__)
        DTU::get().reset();
#endif

        VPE::self().init_state();

        EVENT_TRACE_REINIT();
    }
};

EXTERN_C void init_env(Env *e) {
    e->backend = new EnvUserBackend();
}

}
