/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <base/Common.h>
#include <base/stream/Serial.h>
#include <base/Backtrace.h>
#include <base/Exceptions.h>

namespace m3 {

void Exceptions::init() {
    // TODO
}

void Exceptions::handler(State *state) {
    auto &ser = Serial::get();

    Backtrace::print(ser);

    for(size_t i = 0; i < ARRAY_SIZE(state->r); ++i)
        ser << "  r" << fmt(i, 2) << ": " << fmt(state->r[i], "#0x", 8) << "\n";
    ser << "  lr : " << fmt(state->lr, "#0x", 8) << "\n";

    env()->exit(1);
}

}
