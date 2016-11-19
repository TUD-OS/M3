/**
 * Copyright (C) 2016, René Küttner <rene.kuettner@.tu-dresden.de>
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

#include <base/DTU.h>
#include <base/Env.h>
#include <base/RCTMux.h>
#include <base/Exceptions.h>

#include "../../RCTMux.h"

EXTERN_C void *_vector_table;
EXTERN_C void *_exc_entry;

EXTERN_C void _save(void *state);

namespace RCTMux {

static void install_handler(uint irq, void *handler_func) {
    uint32_t *v = reinterpret_cast<uint32_t*>(&_vector_table);
    size_t off = reinterpret_cast<uint32_t>(handler_func);
    off -= 8 + sizeof(uint32_t) * irq;
    v[irq] = 0xEA000000 | (off >> 2);
}

EXTERN_C void exc_handler(m3::Exceptions::State *state) {
    _save(state);
    m3::DTU::get().clear_irq();
}

void init() {
    for(uint i = 0; i < 8; ++i)
        install_handler(i, &_exc_entry);
}

void *init_state() {
    m3::Env *senv = m3::env();
    // senv->isrs = Exceptions::get_table();

    auto state = reinterpret_cast<m3::Exceptions::State*>(senv->sp) - 1;
    for(size_t i = 0; i < ARRAY_SIZE(state->r); ++i)
        state->r[i] = 0;
    // don't set the stackpointer in crt0
    state->r[1]     = 0xDEADBEEF;
    state->pc       = senv->entry;
    state->cpsr     = 0x13;  // supervisor mode
    state->lr       = 0;

    return state;
}

} /* namespace RCTMux */
