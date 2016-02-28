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

#include <m3/arch/t3/Config.h>
#include <xtensa/coreasm.h>
#include <xtensa/config/specreg.h>

    .data

    .extern _interrupt_handler
    .extern _regstate

    // place a stub at .UserExceptionVector.text that jumps to the
    // interrupt handler
    .section .UserExceptionVector.text, "ax"
    .begin  literal_prefix .UserExceptionVector
    .align  4
_UserExceptionVector:
    wsr     a3, EXCSAVE_1               // preserve content of a3
    movi    a3, _UserExceptionHandler
    jx      a3                          // use jx to allow far jumps
    .end    literal_prefix

    .text

    .literal_position
    .align   1
    .literal LIT_RCTMUX_STACK_TOP, RCTMUX_STACK_TOP

     // check type of interrupt
    .align  4
    .global _UserExceptionHandler
_UserExceptionHandler:
    rsr     a3, EXCCAUSE
    beqi    a3, EXCCAUSE_LEVEL1INTERRUPT, handleInt
    // TODO fail hard for now if we encounter any other interrupt type
    //      than Level1 - this is specific to the simulator
    rsr     a3, EXCSAVE_1
    break   1, 1

handleInt:
    // load address of _regstate into a3
    movi    a3, _regstate
    l32i    a3, a3, 0

saveIntRegs:
    // save register contents to *regstate in order to access them from C
    s32i    a0, a3, 0
    s32i    a1, a3, 4
    s32i    a2, a3, 8
    s32i    a4, a3, 16
    s32i    a5, a3, 20
    s32i    a6, a3, 24
    s32i    a7, a3, 28
    s32i    a8, a3, 32
    s32i    a9, a3, 36
    s32i    a10, a3, 40
    s32i    a11, a3, 44
    s32i    a12, a3, 48
    s32i    a13, a3, 52
    s32i    a14, a3, 56
    s32i    a15, a3, 60
    rsr     a2, EXCSAVE_1
    s32i    a2, a3, 12
    rsr     a2, LBEG
    s32i    a2, a3, 64
    rsr     a2, LEND
    s32i    a2, a3, 68
    rsr     a2, LCOUNT
    s32i    a2, a3, 72
    rsr     a2, SAR
    s32i    a2, a3, 76
    rsr     a2, PS
    s32i    a2, a3, 80
    rsr     a2, EPC_1
    s32i    a2, a3, 84
    movi    a2, 0
    wsr     a2, LCOUNT
    isync

    // save physical register file to stack
    call0   xthal_window_spill_nw

    movi    a3, PS_WOE_MASK + 1
    wsr     a3, PS
    rsync

    // finally, call the C interrupt handler
    movi    a4, _interrupt_handler
    movi    a1, LIT_RCTMUX_STACK_TOP
    mov     a5, a1
    callx4  a4

returnFromInterrupt:

    // first, restore PS
    movi    a3, _regstate
    l32i    a3, a3, 0
    l32i    a4, a3, 80
    wsr     a4, PS
    rsync

    // window may be changed
    movi    a3, _regstate
    l32i    a3, a3, 0

    l32i    a4, a3, 16
    l32i    a5, a3, 20
    l32i    a6, a3, 24
    l32i    a7, a3, 28
    l32i    a8, a3, 32
    l32i    a9, a3, 36
    l32i    a10, a3, 40
    l32i    a11, a3, 44
    l32i    a12, a3, 48
    l32i    a13, a3, 52
    l32i    a14, a3, 56
    l32i    a15, a3, 60

    l32i    a0, a3, 84
    wsr     a0, EPC_1
    l32i    a0, a3, 76
    wsr     a0, SAR
    l32i    a0, a3, 64
    wsr     a0, LBEG
    l32i    a0, a3, 68
    wsr     a0, LEND
    l32i    a0, a3, 72
    wsr     a0, LCOUNT
    isync

    l32i    a0, a3, 0
    l32i    a1, a3, 4
    l32i    a2, a3, 8
    l32i    a3, a3, 12

    rfe

halt:
    j       halt
