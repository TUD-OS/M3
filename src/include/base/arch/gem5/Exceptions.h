/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

namespace m3 {

class Exceptions {
public:
    /* the stack frame for the interrupt-handler */
    struct State {
        /* general purpose registers */
        ulong r15;
        ulong r14;
        ulong r13;
        ulong r12;
        ulong r11;
        ulong r10;
        ulong r9;
        ulong r8;
        ulong rbp;
        ulong rsi;
        ulong rdi;
        ulong rdx;
        ulong rcx;
        ulong rbx;
        ulong rax;
        /* interrupt-number */
        ulong intrptNo;
        /* error-code (for exceptions); default = 0 */
        ulong errorCode;
        /* pushed by the CPU */
        ulong rip;
        ulong cs;
        ulong rflags;
        ulong rsp;
        ulong ss;
    } PACKED;

    typedef void (*isr_func)(State *state);

    static void init();

private:
    static void handler(State *state);
};

}
