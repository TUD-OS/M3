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

#include <m3/cap/RecvGate.h>
#include <m3/cap/VPE.h>
#include <m3/stream/OStream.h>
#include <m3/stream/Serial.h>
#include <m3/Env.h>
#include <m3/DTU.h>
#include <functional>
#include <string.h>

EXTERN_C int main(int argc, char **argv);

namespace m3 {

OStream &operator<<(OStream &os, const Env &senv) {
    static_assert((sizeof(Env) % DTU_PKG_SIZE) == 0, "sizeof(Env) % 8 !=  0");

    os << "core  : " << senv.coreid << "\n";
    os << "argc  : " << senv.argc << "\n";
    os << "argv  : " << fmt((void*)senv.argv, "p") << "\n";
    os << "sp    : " << fmt(senv.sp, "p") << "\n";
    os << "entry : " << fmt(senv.entry, "p") << "\n";
    os << "lambda: " << fmt(senv.lambda, "p") << "\n";
    os << "pgsess: " << senv.pager_sess << "\n";
    os << "pggate: " << senv.pager_gate << "\n";
    os << "mntlen: " << senv.mount_len << "\n";
    os << "mounts: " << fmt(senv.mounts, "p") << "\n";
    os << "eps   : " << fmt(senv.eps, "p") << "\n";
    os << "caps  : " << fmt(senv.caps, "p") << "\n";
    os << "exit  : " << fmt(senv.exit, "p") << "\n";
    return os;
}

void Env::run() {
    Env *e = env();

    int res;
    if(e->lambda) {
        e->reinit();

        std::function<int()> *f = reinterpret_cast<std::function<int()>*>(e->lambda);
        EVENT_TRACER_lambda_main();
        res = (*f)();
    }
    else {
        e->init();

        res = main(e->argc, e->argv);
    }

    ::exit(res);
    UNREACHED;
}

void Env::init() {
    pre_init();

    bool kernel = argv && strstr(argv[0], "kernel") != nullptr;
    if(!kernel) {
        // wait until the kernel has initialized our core
        volatile Env *senv = this;
        while(senv->coreid == 0)
            ;
    }
    // the kernel is always on a predefined core
    else
        coreid = KERNEL_CORE;

    def_recvbuf = new RecvBuf(RecvBuf::bindto(
        DTU::DEF_RECVEP, reinterpret_cast<void*>(DEF_RCVBUF), DEF_RCVBUF_ORDER, 0));
    def_recvgate = new RecvGate(RecvGate::create(def_recvbuf));

    // TODO argv is always present, isn't it?
    Serial::init(argv ? argv[0] : "Unknown", coreid);

    post_init();
}

void Env::reinit() {
    // wait until the kernel has initialized our core
    volatile Env *senv = this;
    while(senv->coreid == 0)
        ;

#if defined(__t3__)
    // set default receive buffer again
    DTU::get().configure_recv(def_recvbuf->epid(), reinterpret_cast<word_t>(def_recvbuf->addr()),
        def_recvbuf->order(), def_recvbuf->msgorder(), def_recvbuf->flags());
#endif

    Serial::init(argv ? argv[0] : "Unknown", senv->coreid);
    EPMux::get().reset();
#if defined(__t2__)
    DTU::get().reset();
#endif

    VPE::self().init_state();

    EVENT_TRACE_REINIT();
}

}
