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
#include <m3/DTU.h>
#include <m3/Env.h>
#include <m3/util/Math.h>
#include <m3/util/Sync.h>
#include <m3/util/Profile.h>
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
    alignas(DTU_PKG_SIZE) uint32_t addr;
    size_t offset = 0;

    // wait for kernel
    while (flag_is_set(SIGNAL) && !flag_is_set(ERROR))
        ;

    if (flag_is_set(ERROR))
        return;

    // store state
    //mem_write(RCTMUX_STORE_EP, (void*)&_state, sizeof(_state), &offset);

    // success
    flag_unset(STORE);

    // on gem5 store is handled via libm3
}

void restore() {
    alignas(DTU_PKG_SIZE) uint32_t addr;
    size_t offset = 0;

    while (flag_is_set(SIGNAL) && !flag_is_set(ERROR))
        ;

    if (flag_is_set(ERROR))
        return;

    // read state
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
