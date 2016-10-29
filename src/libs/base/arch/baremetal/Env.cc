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

#include <base/stream/OStream.h>
#include <base/stream/Serial.h>
#include <base/tracing/Tracing.h>
#include <base/Env.h>
#include <base/DTU.h>
#include <functional>
#include <string.h>

EXTERN_C void __cxa_finalize(void *);
EXTERN_C void init_env(m3::Env *env);
EXTERN_C int main(int argc, char **argv);

namespace m3 {

OStream &operator<<(OStream &os, const Env &senv) {
#if defined(__t2__) || defined(__t3__)
    static_assert((sizeof(Env) % DTU_PKG_SIZE) == 0, "sizeof(Env) % 8 !=  0");
#endif

    os << "pe    : " << senv.pe << "\n";
    os << "argc  : " << senv.argc << "\n";
    os << "argv  : " << fmt((void*)senv.argv, "p") << "\n";
    os << "sp    : " << fmt(senv.sp, "p") << "\n";
    os << "entry : " << fmt(senv.entry, "p") << "\n";
    os << "lambda: " << fmt(senv.lambda, "p") << "\n";
    os << "pgsess: " << senv.pager_sess << "\n";
    os << "pggate: " << senv.pager_gate << "\n";
    os << "mounts: " << senv.mounts << "\n";
    os << "mntlen: " << senv.mounts_len << "\n";
    os << "fds   : " << senv.fds << "\n";
    os << "fdslen: " << senv.fds_len << "\n";
    os << "mounts: " << fmt(senv.mounts, "p") << "\n";
    os << "eps   : " << fmt(senv.eps, "p") << "\n";
    os << "caps  : " << fmt(senv.caps, "p") << "\n";
    os << "exit  : " << fmt(senv.exitaddr, "p") << "\n";
    return os;
}

void Env::run() {
    Env *e = env();

    int res;
    if(e->lambda) {
        e->backend->reinit();

        EVENT_TRACER_Lambda();
        std::function<int()> *f = reinterpret_cast<std::function<int()>*>(e->lambda);
        res = (*f)();
    }
    else {
        init_env(e);
        e->pre_init();
        e->backend->init();
        e->post_init();

        EVENT_TRACER_Main();
        res = main(e->argc, e->argv);
    }

    e->exit(res);
    UNREACHED;
}

USED void Env::exit(int code) {
    pre_exit();
    __cxa_finalize(nullptr);
    backend->exit(code);
    entry = 0;
    jmpto(exitaddr);
}

}
