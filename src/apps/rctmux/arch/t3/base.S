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
#include <xtensa/config/specreg.h>

    .data

    .extern _store
    .extern _regstate
    .extern _irqaddr
    .extern _start
    .extern _reset

    .literal_position
    .align  1
    .literal LIT_RCTMUX_FLAGS, RCTMUX_FLAGS_LOCAL
    .literal LIT_RCTMUX_FLAG_SIGNAL, RCTMUX_FLAG_SIGNAL
    .literal LIT_RCTMUX_FLAG_STORE, RCTMUX_FLAG_STORE

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

    // make space for a small stack
    .data
    .align 16                           // required by xtensa ABI
    .global _RCTMuxStackBase
    .global _RCTMuxStackEnd
_RCTMuxStackBase:
    .space 64, 0
_RCTMuxStackEnd:
    .space 16, 0
_RCTMuxStackStart:

    .text

     // filter level 1 interrupts
    .align  4
    .global _UserExceptionHandler
_UserExceptionHandler:

    rsr     a3, EXCCAUSE
    beqi    a3, EXCCAUSE_LEVEL1INTERRUPT, _store
    // TODO fail hard for now if we encounter any other interrupt type
    //      than Level1 - this is specific to the simulator
    rsr     a3, EXCSAVE_1
    break   1, 1

    // --- store ---

_store:

    // load address of _regstate into a3
    movi    a3, _regstate
    l32i    a3, a3, 0

    // save register contents to *regstate to access them from within C++
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

    // reset DTU's irq register to make further interrupts work
    // FIXME: why do we have to do this?
    l32r    a3, _irqaddr
    movi    a4, 0
    s32i.n  a4, a3, 0

    // switch to rctmux' stack
    movi    a1, _RCTMuxStackEnd

    // check if a context switch has requested, if not skip over
    l32r    a3, LIT_RCTMUX_FLAGS
    l32i.n  a3, a3, 0
    l32r    a4, LIT_RCTMUX_FLAG_SIGNAL
    and     a4, a3, a4
    beqz    a4, _skip_store

    // if the store flag is not set, we proceed to reset
    l32r    a4, LIT_RCTMUX_FLAG_STORE
    and     a4, a3, a4
    beqz    a4, _do_reset

    // finally, call the storage-phase handler
    movi    a4, _store_context
    movi    a1, _RCTMuxStackEnd
    callx4  a4

    // note: normally, the kernel would just reset the core, however,
    // on t3 the reset from remote is not possible, thus we fake it
_do_reset:
    movi    a4, _reset
    callx4  a4                          // will not return

    // if we return here, it means no store has been performed which
    // would have lead to a core reset, thus we just return
_skip_store:
    rfe

    // --- restore ---

    .align 4
    .global _restore
_restore:

    movi    a4, _restore_context
    movi    a1, _RCTMuxStackEnd
    callx4  a4

    // if the restoration was successful, the processor state
    // will become those during the interrupt (exception mode) as soon as we
    // restore the PS register - the application will continue to run at the
    // address in the EPC register after return from exception (rfe)

    // first, restore PS
    movi    a3, _regstate
    l32i    a3, a3, 0
    l32i    a4, a3, 80
    wsr     a4, PS
    rsync

    // window could have been changed
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
