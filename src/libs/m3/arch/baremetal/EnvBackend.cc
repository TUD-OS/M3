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

#include <base/Common.h>
#include <base/stream/Serial.h>
#include <base/Backtrace.h>
#include <base/Env.h>

#include <m3/com/RecvGate.h>
#include <m3/com/RecvBuf.h>
#include <m3/stream/Standard.h>
#include <m3/Syscalls.h>
#include <m3/UserWorkLoop.h>
#include <m3/VPE.h>

namespace m3 {

class EnvUserBackend : public BaremetalEnvBackend {
public:
    explicit EnvUserBackend() {
        _workloop = new UserWorkLoop();
    }

    virtual void init() override {
        // wait until the kernel has initialized our PE
        volatile Env *senv = env();
        while(senv->pe == 0)
            ;

        // TODO argv is always present, isn't it?
        Serial::init(env()->argv ? env()->argv[0] : "Unknown", env()->pe);
    }

    virtual void reinit() override {
        // wait until the kernel has initialized our PE
        volatile Env *senv = env();
        while(senv->pe == 0)
            ;

#if defined(__t3__)
        // set default receive buffer again
        RecvBuf &def = RecvBuf::def();
        DTU::get().configure_recv(def.ep(), reinterpret_cast<word_t>(def.addr()),
            def.order(), def.msgorder(), def.flags());
#endif

        Serial::init(env()->argv ? env()->argv[0] : "Unknown", senv->pe);
        EPMux::get().reset();
#if defined(__t2__)
        DTU::get().reset();
#endif

        VPE::self().init_state();
        VPE::self().init_fs();

        EVENT_TRACE_REINIT();
    }

    void report_idle() override {
        SendGate sg(ObjCap::INVALID, 0, nullptr, DTU::SYSC_SEP);
        send_vmsg(sg, m3::KIF::Syscall::IDLE);
    }

    void exit(int code) override {
        Syscalls::get().exit(code);
    }
};

EXTERN_C void init_env(Env *e) {
    e->backend = new EnvUserBackend();
}

}
