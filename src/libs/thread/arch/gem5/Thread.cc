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
    // put argument in rdi and function to return to on the stack
    // take care of alignment for SSE
    regs->rdi = reinterpret_cast<word_t>(arg);
    stack[T_STACK_WORDS - 4] = reinterpret_cast<word_t>(func);
    regs->rsp = reinterpret_cast<word_t>(stack + T_STACK_WORDS - 5);
    regs->rbp = regs->rsp;
    stack[T_STACK_WORDS - 5] = regs->rbp;
}

}
