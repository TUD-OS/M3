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
#include <base/DTU.h>
#include <base/Env.h>
#include <base/Exceptions.h>
#include <base/RCTMux.h>

#if defined(__x86_64__)
#   include "arch/gem5-x86_64/VMA.h"
#endif
#include "RCTMux.h"
#include "Print.h"

EXTERN_C void *rctmux_stack;
EXTERN_C void _start();

namespace RCTMux {

enum Status {
    INITIALIZED = 1,
    STARTED     = 2,
    RESUMING    = 4,
};

static int status = 0;
#if defined(__arm__)
static void *state_ptr;
#else
static m3::Exceptions::State state;
#endif

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

void init() {
    // if we're here for the first time, setup exception handling
    if(!(status & INITIALIZED)) {
        Arch::init();
        status |= INITIALIZED;
    }
}

void sleep() {
    Arch::sleep();
}

uint64_t report_time() {
    return rctmux_flags[0];
}

void *ctxsw_protocol(void *s, bool inpf) {
    uint64_t flags = flags_get();

    if(flags & m3::RESTORE) {
        s = restore();
        return s;
    }

    if(flags & m3::STORE) {
        // if we're currently handling a PF, don't do the store here
        if(inpf)
            return s;

#if defined(__x86_64__)
        VMA::abort_pf();
#endif
        if(s)
            save(s);
        signal();

        // stay here until reset
        Arch::wait_for_reset();
        UNREACHED;
    }

    if(flags & m3::WAITING) {
        signal();

        // no application anymore; only reset that if the kernel actually requested that
        // because it might happen that we are waked up by a message before the kernel has written
        // the flags register. in this case, we don't want to lose the application.
        status &= ~STARTED;

#if defined(__x86_64__)
        // resume the PF handling, if there is any
        if(!inpf) {
            VMA::execute_fsm(nullptr);
            if(flags_get() & m3::RESTORE)
                return restore();
        }
#endif
    }

    return s;
}

void ctxsw_resume() {
    if(status & RESUMING) {
        Arch::resume();
        status &= ~RESUMING;
    }
}

static void save(UNUSED void *s) {
    Arch::abort();

#if defined(__arm__)
    state_ptr = s;
#else
    static_assert(sizeof(state) % sizeof(word_t) == 0, "State not word-sized");
    word_t *words_src = reinterpret_cast<word_t*>(s);
    word_t *words_dst = reinterpret_cast<word_t*>(&state);
    for(size_t i = 0; i < sizeof(state) / sizeof(word_t); ++i)
        words_dst[i] = words_src[i];
#endif
}

static void *restore() {
    uint64_t flags = flags_get();

    // notify the kernel as early as possible
    signal();

    // first, resume PF handling, if necessary
#if defined(__x86_64__)
    VMA::execute_fsm(nullptr);
#endif

    m3::Env *senv = m3::env();
    // remember the current PE (might have changed since last switch)
    senv->pe = flags >> 32;

    void *res;
    auto *stacktop = reinterpret_cast<m3::Exceptions::State*>(&rctmux_stack) - 1;
    if(!(status & STARTED)) {
        // if we get here, there is an application to jump to

        // remember exit location
        senv->exitaddr = reinterpret_cast<uintptr_t>(&_start);

        // initialize the state to be able to resume from it
        res = Arch::init_state(stacktop);
        status |= STARTED;
    }
    else {
#if defined(__arm__)
        res = state_ptr;
#else
        word_t *words_src = reinterpret_cast<word_t*>(&state);
        word_t *words_dst = reinterpret_cast<word_t*>(stacktop);
        for(size_t i = 0; i < sizeof(state) / sizeof(word_t); ++i)
            words_dst[i] = words_src[i];
        res = stacktop;
#endif
    }
    status |= RESUMING;

    return res;
}

static void signal() {
    m3::CPU::memory_barrier();
    // tell the kernel that we are ready
    flags_set(m3::SIGNAL);
}

}
