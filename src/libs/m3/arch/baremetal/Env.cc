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

#include <m3/stream/OStream.h>
#include <m3/stream/Serial.h>
#include <m3/tracing/Tracing.h>
#include <m3/Env.h>
#include <m3/DTU.h>
#include <functional>
#include <string.h>

EXTERN_C void init_env(m3::Env *env);
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
        e->backend->reinit();

        std::function<int()> *f = reinterpret_cast<std::function<int()>*>(e->lambda);
        EVENT_TRACER_lambda_main();
        res = (*f)();
    }
    else {
        init_env(e);
        e->pre_init();
        e->backend->init();
        e->post_init();

        res = main(e->argc, e->argv);
    }

    ::exit(res);
    UNREACHED;
}

}
