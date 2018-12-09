/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#define SLOG(lvl, msg)  LOG(ServiceLog, lvl, msg)

namespace m3 {

class ServiceLog {
    ServiceLog() = delete;

public:
    enum Level {
        KEYB        = 1 << 0,
        FS          = 1 << 1,
        PAGER       = 1 << 2,
        PIPE        = 1 << 3,
        HASH        = 1 << 4,
        VTERM       = 1 << 5,
        IDE         = 1 << 6,
        IDE_ALL     = 1 << 7,
        LOADGEN     = 1 << 8,
        NIC         = 1 << 9,
        NET         = 1 << 10,
    };

    static const int level = 0;
};

}
