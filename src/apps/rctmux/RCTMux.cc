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

static void *restore();

static void *state;

EXTERN_C void *_start_app() {
    uint64_t flags = flags_get();
    // if we're here for the first time, setup exception handling
    if(flags & m3::INIT)
        init();

    if(flags & m3::RESTORE)
        return restore();

    if(flags)
        flags_set(m3::SIGNAL);

    return nullptr;
}

EXTERN_C void _sleep() {
    sleep();
}

EXTERN_C void _save(void *s) {
    assert(flags_get() & m3::STORE);

    bool can_block = save();

    state = s;

    uint64_t flags = m3::SIGNAL;
    if (can_block && m3::env()->idle_active)
        flags |= m3::BLOCK;
    flags_set(flags);
}

static void *restore() {
    uint64_t flags = flags_get();

    m3::Env *senv = m3::env();
    // remember the current core id (might have changed since last switch)
    senv->coreid = flags >> 32;
    senv->idle_report = flags & m3::REPORT;

    if(flags & m3::INIT) {
        // if we get here, there is an application to jump to
        assert(senv->entry != 0);

        // remember exit location
        senv->exitaddr = reinterpret_cast<uintptr_t>(&_start);

        // initialize the state to be able to resume from it
        state = init_state();
    }
    else
        resume();

    // tell the kernel that we are ready
    flags_set(m3::SIGNAL);

    return state;
}

}
