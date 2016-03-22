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

#include <base/stream/Serial.h>
#include <base/Env.h>

#define LLOG(lvl, expr)                                     \
    do {                                                    \
        if(m3::LibLog::level & (m3::LibLog::lvl))           \
            m3::Serial::get() << expr << '\n';              \
    }                                                       \
    while(0)

namespace m3 {

class LibLog {
    LibLog() = delete;

public:
    enum Level {
        SYSC        = 1 << 0,
        DTU         = 1 << 1,
        DTUERR      = 1 << 2,
        IPC         = 1 << 3,
        TRACE       = 1 << 4,
        IRQS        = 1 << 5,
        SHM         = 1 << 6,
    };

    static const int level = 0;
};

}
