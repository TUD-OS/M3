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

#include <base/CPU.h>
#include <base/Env.h>
#include <base/RCTMux.h>

#include "RCTMux.h"
#include "Print.h"

EXTERN_C void _start();

namespace RCTMux {

enum Status {
    INITIALIZED = 1,
    STARTED     = 2,
};

static int status = 0;
static void *state = nullptr;

static void save(void *s);
static void *restore();
static void signal();

__attribute__((section(".rctmux"))) static volatile uint64_t rctmux_flags[2];

static inline uint64_t flags_get() {
    return rctmux_flags[1];
}

static inline void flags_set(uint64_t flags) {
    rctmux_flags[1] = flags;
}

void *init() {
    // if we're here for the first time, setup exception handling
    if(!(status & INITIALIZED)) {
        Arch::init();
        status |= INITIALIZED;
    }

    return ctxsw_protocol(nullptr);
}

void sleep() {
    Arch::sleep();
}

void *ctxsw_protocol(void *s) {
    uint64_t flags = flags_get();

    if(flags & m3::RESTORE) {
        s = restore();
        Arch::reset_sp();
        return s;
    }

    if(flags & m3::STORE) {
        if(s)
            save(s);

        // stay here until reset
        Arch::save_sp();
        Arch::enable_ints();
        while(1)
            sleep();
        UNREACHED;
    }

    if(flags & m3::WAITING) {
        signal();

        // no application anymore; only reset that if the kernel actually requested that
        // because it might happen that we are waked up by a message before the kernel has written
        // the flags register. in this case, we don't want to lose the application.
        status &= ~STARTED;
        state = nullptr;
    }

    Arch::reset_sp();
    return s;
}

static void save(void *s) {
    Arch::abort();

    state = s;

    signal();
}

static void *restore() {
    uint64_t flags = flags_get();

    m3::Env *senv = m3::env();
    // remember the current PE (might have changed since last switch)
    senv->pe = flags >> 32;

    if(!(status & STARTED)) {
        // if we get here, there is an application to jump to

        // remember exit location
        senv->exitaddr = reinterpret_cast<uintptr_t>(&_start);

        // initialize the state to be able to resume from it
        state = Arch::init_state();
        status |= STARTED;
    }
    else
        Arch::resume();

    signal();
    return state;
}

static void signal() {
    m3::CPU::memory_barrier();
    // tell the kernel that we are ready
    flags_set(m3::SIGNAL);
}

}
