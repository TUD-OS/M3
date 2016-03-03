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

#include <m3/arch/t3/RCTMux.h>
#include <xtensa/coreasm.h>

    .text

    .extern  _loop
    .extern  _setup
    .extern  _restore

    // declare literals
    .literal_position
    .align   1
    .literal LIT_STACK_TOP, STACK_TOP
    .literal LIT_PS, 0x00040020
    .literal LIT_RCTMUX_FLAGS, RCTMUX_FLAGS_LOCAL
    .literal LIT_RCTMUX_FLAG_SIGNAL, RCTMUX_FLAG_SIGNAL
    .literal LIT_RCTMUX_FLAG_RESTORE, RCTMUX_FLAG_RESTORE

    // declare global symbols
    .align  4
    .global _start
    .type   _start, @function

_start:
    movi.n  a0, 0                           // mark the first stack frame
    l32r    a1, LIT_STACK_TOP               // initialize stack pointer

    // initialize processor state
    l32r    a3, LIT_PS
    wsr     a3, ps
    rsync

    // check if we have to handle a restore
    l32r    a4, LIT_RCTMUX_FLAG_SIGNAL
    l32r    a3, LIT_RCTMUX_FLAG_RESTORE
    or      a3, a3, a4
    l32r    a4, LIT_RCTMUX_FLAGS
    l32i.n  a4, a4, 0
    ball    a4, a3, _restore

    // initialize
    call4   _setup

    // enable interrupts
    movi.n  a3, 1
    wsr     a3, intenable

    // wait until we are signaled
    call4   _loop

halt:
    // this should never be reached
    j       halt
