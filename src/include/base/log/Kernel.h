/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/log/Log.h>

#define KLOG(lvl, msg)  LOG(KernelLog, lvl, msg)

namespace m3 {

class KernelLog {
    KernelLog() = delete;

public:
    enum Level {
        INFO            = 1 << 0,
        KENV            = 1 << 1,
        ERR             = 1 << 2,
        MEM             = 1 << 3,
        SYSC            = 1 << 4,
        PTES            = 1 << 5,
        VPES            = 1 << 6,
        EPS             = 1 << 7,
        SERV            = 1 << 8,
        SLAB            = 1 << 9,
        TIMEOUTS        = 1 << 10,
        CTXSW           = 1 << 11,
        CTXSW_STATES    = 1 << 12,
        SQUEUE          = 1 << 13,
        UPCALLS         = 1 << 14,
    };

    static const int level = INFO | ERR;
};

}
