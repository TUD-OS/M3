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

#include <base/PE.h>

namespace m3 {

struct KernelEnv {
    static const size_t MAX_MODS        = 8;
    static const size_t MAX_PES         = 64;

    uintptr_t mods[MAX_MODS];
    PE pes[MAX_PES];
} PACKED;

}

#if defined(__host__)
#   include <base/arch/host/Env.h>
#else
#   include <base/arch/baremetal/Env.h>
#endif
