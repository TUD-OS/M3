/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/CPU.h>
#include <base/DTU.h>

namespace m3 {

class Time {
    Time() = delete;

    static const unsigned START_TSC     = 0x1FF10000;
    static const unsigned STOP_TSC      = 0x1FF20000;

public:
    static cycles_t start(unsigned id = 0);
    static cycles_t stop(unsigned id = 0);
};

#if defined(__t3__)
inline cycles_t Time::start(unsigned id) {
    CPU::compiler_barrier();
    DTU::get().debug_msg(START_TSC | id);
    return 0;
}

inline cycles_t Time::stop(unsigned id) {
    DTU::get().debug_msg(STOP_TSC | id);
    CPU::compiler_barrier();
    return 0;
}
#endif

}
