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

#include <m3/stream/FStream.h>

namespace m3 {

static const fd_t STDIN_FD      = 0;
static const fd_t STDOUT_FD     = 1;
static const fd_t STDERR_FD     = 2;

extern FStream cin;
extern FStream cout;
extern FStream cerr;

static inline void appenderr(FStream &os) {
    if(Errors::last != Errors::NO_ERROR)
        os << ": " << Errors::to_string(Errors::last);
    os << "\n";
}

#define errmsg(expr) do {       \
        m3::cerr << expr;       \
        appenderr(m3::cerr);    \
    }                           \
    while(0)

#define exitmsg(expr) do {      \
        errmsg(expr);           \
        ::exit(1);              \
    }                           \
    while(0)

}
