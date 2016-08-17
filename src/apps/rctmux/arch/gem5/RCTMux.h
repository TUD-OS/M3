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

#pragma once

#include <base/Common.h>

#include <m3/RCTMux.h>

static unsigned _RCTMUX_FLAGS;

namespace RCTMux {

inline void jump_to_start(const uintptr_t ptr) {
    asm volatile (
        "jmp *%0"
        : : "r"(ptr)
    );
}

inline void jump_to_app(const uintptr_t ptr, const word_t sp) {
    // tell crt0 to set this stackpointer
    asm volatile (
        "mov %2, %%rsp;"
        "jmp *%1;"
        : : "a"(0xDEADBEEF), "r"(ptr), "r"(sp)
    );
}

inline void flag_set(const m3::RCTMUXCtrlFlag flag) {
    _RCTMUX_FLAGS |= flag;
}

inline void flag_unset(const m3::RCTMUXCtrlFlag flag) {
    _RCTMUX_FLAGS ^= flag;
}

inline void flags_reset() {
    _RCTMUX_FLAGS = 0;
}

inline bool flag_is_set(const m3::RCTMUXCtrlFlag flag) {
    return (_RCTMUX_FLAGS & flag);
}

} /* namespace RCTMux */
