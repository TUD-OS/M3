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
#include <base/Config.h>

#include <m3/RCTMux.h>

namespace RCTMux {

inline void jump_to_app(const uintptr_t ptr, const word_t sp) {
    // tell crt0 to set this stackpointer
    reinterpret_cast<word_t*>(STACK_TOP)[-1] = 0xDEADBEEF;
    reinterpret_cast<word_t*>(STACK_TOP)[-2] = sp;

    register word_t a2 __asm__ ("a2") = ptr;
    asm volatile ( "jx    %0;" : : "a"(a2) );
}

inline void flag_set(const m3::RCTMUXCtrlFlag flag) {
    *((volatile unsigned *)RCTMUX_FLAGS_LOCAL) |= flag;
}

inline void flag_unset(const m3::RCTMUXCtrlFlag flag) {
    *((volatile unsigned *)RCTMUX_FLAGS_LOCAL) ^= flag;
}

inline void flags_reset() {
    *((volatile unsigned *)RCTMUX_FLAGS_LOCAL) = 0;
}

inline bool flag_is_set(const m3::RCTMUXCtrlFlag flag) {
    return (*((volatile unsigned *)RCTMUX_FLAGS_LOCAL) & flag);
}

} /* namespace RCTMux */
