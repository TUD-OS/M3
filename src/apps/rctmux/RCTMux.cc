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

#include <base/util/Sync.h>
#include <base/Env.h>
#include <base/RCTMux.h>

#include <assert.h>

#include "Debug.h"
#include "RCTMux.h"

EXTERN_C void _start();

namespace RCTMux {

enum Status {
    INITIALIZED = 1,
    STARTED     = 2,
};

static void *restore();

static uint status = 0;
static void *state = nullptr;

EXTERN_C void *_start_app() {
    uint64_t flags = flags_get();

    // if we're here for the first time, setup exception handling
    if(!(status & INITIALIZED)) {
        init();
        status |= INITIALIZED;
    }

    if(flags & m3::RESTORE)
        return restore();

    if(flags & m3::WAITING) {
        m3::Sync::memory_barrier();
        flags_set(m3::SIGNAL);

        // no application anymore; only reset that if the kernel actually requested that
        // because it might happen that we are waked up by a message before the kernel has written
        // the flags register. in this case, we don't want to lose the application.
        status &= ~STARTED;
        state = nullptr;
    }

    return nullptr;
}

EXTERN_C void _sleep() {
    m3::Sync::memory_barrier();
    sleep();

    // it might happen that IRQs are issued late, so that, if we're idling, the STORE flag is set
    // when we get here. just wait until the IRQ is issued.
    m3::Sync::memory_barrier();
    while(flags_get() & m3::STORE)
        ;
}

EXTERN_C void _save(void *s) {
    assert(flags_get() & m3::STORE);
    assert(flags_get() & m3::WAITING);

    save();

    state = s;

    m3::Sync::memory_barrier();
    flags_set(m3::SIGNAL);
}

static void *restore() {
    uint64_t flags = flags_get();
    assert(flags & m3::WAITING);

    m3::Env *senv = m3::env();
    // remember the current core id (might have changed since last switch)
    senv->coreid = flags >> 32;

    if(!(status & STARTED)) {
        // if we get here, there is an application to jump to
        assert(senv->entry != 0);

        // remember exit location
        senv->exitaddr = reinterpret_cast<uintptr_t>(&_start);

        // initialize the state to be able to resume from it
        state = init_state();
        status |= STARTED;
    }
    else
        resume();

    // tell the kernel that we are ready
    m3::Sync::memory_barrier();
    flags_set(m3::SIGNAL);

    return state;
}

}
