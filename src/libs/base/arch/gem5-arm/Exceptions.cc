/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

static const char *exNames[] = {
    /* 0x00 */ "Reset",
    /* 0x01 */ "Undefined Instruction",
    /* 0x02 */ "Software Interrupt",
    /* 0x03 */ "Prefetch Abort",
    /* 0x04 */ "Data Abort",
    /* 0x05 */ "Reserved",
    /* 0x06 */ "IRQ",
    /* 0x07 */ "FIQ",
};

void Exceptions::init() {
    if(env()->isrs) {
        auto funcs = reinterpret_cast<Exceptions::isr_func*>(env()->isrs);
        for(size_t i = 0; i < ARRAY_SIZE(exNames); ++i) {
            if(i != 6)
                funcs[i] = handler;
        }
    }
}

void *Exceptions::handler(State *state) {
    auto &ser = Serial::get();
    ser << "Interruption @ " << fmt(state->pc, "p") << "\n";
    ser << "  vector: ";
    if(state->vector < ARRAY_SIZE(exNames))
        ser << exNames[state->vector];
    else
        ser << "<unknown> (" << state->vector << ")";
    ser << "\n";

    Backtrace::print(ser);

    ser << "Registers:\n";
    for(size_t i = 0; i < ARRAY_SIZE(state->r); ++i)
        ser << "   r" << fmt(i, "0", 2) << ": " << fmt(state->r[i], "#0x", 8) << "\n";
    ser << "  cpsr: " << fmt(state->cpsr, "#0x", 8) << "\n";
    ser << "    lr: " << fmt(state->lr, "#0x", 8) << "\n";

    env()->exit(1);
    return state;
}

}
