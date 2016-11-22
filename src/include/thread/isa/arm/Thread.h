/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
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

#ifdef __cplusplus
#include <base/Types.h>

namespace m3 {

typedef void (*_thread_func)(void*);

struct Regs {
    word_t r0;
    word_t r4;
    word_t r5;
    word_t r6;
    word_t r7;
    word_t r8;
    word_t r9;
    word_t r10;
    word_t r11;
    word_t r13;
    word_t r14;
    word_t cpsr;
};

enum {
    T_STACK_WORDS = 512
};

void thread_init(_thread_func func, void *arg, Regs *regs, word_t *stack);
extern "C"  bool thread_save(Regs *regs);
extern "C" bool thread_resume(Regs *regs);

}

#endif
