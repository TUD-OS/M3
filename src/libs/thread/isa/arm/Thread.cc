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

#include <thread/Thread.h>

namespace m3 {

void thread_init(Thread::thread_func func, void *arg, Regs *regs, word_t *stack) {
    regs->r0 = reinterpret_cast<word_t>(arg);                             // arg
    regs->r13 = reinterpret_cast<word_t>(stack + T_STACK_WORDS - 2);      // sp
    regs->r11 = 0;                                                        // fp
    regs->r14 = reinterpret_cast<word_t>(func);                           // lr
    regs->cpsr = 0x13;  // supervisor mode
}

}
