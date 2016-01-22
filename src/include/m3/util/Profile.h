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

#include <m3/util/Sync.h>
#include <m3/DTU.h>

namespace m3 {

class Profile {
    Profile() = delete;

public:
    static cycles_t start(unsigned id = 0);
    static cycles_t stop(unsigned id = 0);
};

#if defined(__t3__)
#   define START_TSC           0xFFF10000
#   define STOP_TSC            0xFFF20000

inline cycles_t Profile::start(unsigned id) {
    Sync::compiler_barrier();
    DTU::get().debug_msg(START_TSC | id);
    return 0;
}

inline cycles_t Profile::stop(unsigned id) {
    DTU::get().debug_msg(STOP_TSC | id);
    Sync::compiler_barrier();
    return 0;
}
#endif

}
