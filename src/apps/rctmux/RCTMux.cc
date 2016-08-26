/**
 * Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
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

#include <m3/RCTMux.h>
#include <base/DTU.h>
#include <base/Env.h>
#include <m3/Syscalls.h>

#include "arch/gem5/Exceptions.h"
#include "RCTMux.h"
#include "Debug.h"

using namespace m3;

EXTERN_C void _start();

namespace RCTMux {

static m3::Exceptions::State *state;
static DTU::reg_t cmdreg;

EXTERN_C bool _init() {
    uint64_t flags = flags_get();

    // if we're here for the first time, setup exception handling
    if(flags & INIT)
        setup();

    return flags != 0;
}

EXTERN_C m3::Exceptions::State *_start_app() {
    if(!flag_is_set(RESTORE)) {
        // tell the kernel that we are ready
        // TODO only do that if the kernel knows about that (not after exit)
        flags_set(SIGNAL);
        return nullptr;
    }

    m3::Env *senv = m3::env();
    senv->coreid = flags_get() >> 32;

    if(flag_is_set(INIT)) {
        // if there is no application to run yet, go back to sleep
        assert(senv->entry != 0);

        senv->isrs = ::Exceptions::get_table();

        // remember exit location
        senv->exitaddr = reinterpret_cast<uintptr_t>(&_start);

        // put state at the stack top
        state = reinterpret_cast<m3::Exceptions::State*>(senv->sp) - 1;

        // init state
        state->rax = 0;
        state->rbx = 0;
        state->rcx = 0;
        state->rdx = 0;
        state->rsi = 0;
        state->rdi = 0;
        state->r8  = 0;
        state->r9  = 0;
        state->r10 = 0;
        state->r11 = 0;
        state->r12 = 0;
        state->r13 = 0;
        state->r14 = 0;
        state->r15 = 0;

        state->cs  = (::Exceptions::SEG_CODE << 3) | 3;
        state->ss  = (::Exceptions::SEG_DATA << 3) | 3;
        state->rip = senv->entry;
        state->rsp = reinterpret_cast<uintptr_t>(state);
        state->rbp = state->rsp;
        state->rflags = 0x200;  // enable interrupts
    }
    else
        DTU::get().retry(cmdreg);

    // tell the kernel that we are ready
    flags_set(SIGNAL);

    return state;
}

EXTERN_C void _interrupt_handler(m3::Exceptions::State *s) {
    DTU::get().abort(DTU::ABORT_CMD | DTU::ABORT_VPE, &cmdreg);

    state = s;

    flags_set(SIGNAL);
}

} /* namespace RCTMux */
