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

#include <m3/RCTMux.h>
#include <base/DTU.h>
#include <base/Env.h>
#include <base/util/Math.h>
#include <base/util/Sync.h>
#include <base/util/Profile.h>
#include <string.h>

#include "RCTMux.h"

using namespace m3;

#define RCTMUX_MAGIC       0x42C0FFEE

EXTERN_C void _start();
EXTERN_C void isr64();

volatile static struct alignas(DTU_PKG_SIZE) {
    word_t magic;
} _state;

namespace RCTMux {

void setup() {
    _state.magic = RCTMUX_MAGIC;
    //flags_reset();

    m3::env()->isr64_handler = reinterpret_cast<uintptr_t>(&isr64);
}

void init() {
    // tell kernel that we are ready
    flag_set(SIGNAL);
}

void finish() {
    flags_reset();
}

void store() {
    // wait for kernel
    while (flag_is_set(SIGNAL) && !flag_is_set(ERROR))
        ;

    if (flag_is_set(ERROR))
        return;

    // store state
    //size_t offset = 0;
    //mem_write(RCTMUX_STORE_EP, (void*)&_state, sizeof(_state), &offset);

    // success
    flag_unset(STORE);

    // on gem5 store is handled via libm3
}

void restore() {
    while (flag_is_set(SIGNAL) && !flag_is_set(ERROR))
        ;

    if (flag_is_set(ERROR))
        return;

    // read state
    //size_t offset = 0;
    //mem_read(RCTMUX_RESTORE_EP, (void*)&_state, sizeof(_state), &offset);

    if (_state.magic != RCTMUX_MAGIC) {
        flag_set(ERROR);
        return;
    }

    // success
    flag_unset(RESTORE);
}

void reset() {
    // simulate reset since resetting the PE from kernel side is not
    // currently supported for t3
    // TODO
}

void set_idle_mode() {
    // reset program entry
    m3::env()->entry = 0;
    // set epc (exception program counter) to jump into idle mode
    // when returning from exception
    //_state.cpu_regs[EPC_REG] = (word_t*)&_start;
    jump_to_start((uintptr_t)&_start);
}

} /* namespace RCTMux */
