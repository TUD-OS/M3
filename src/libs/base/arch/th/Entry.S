#
# Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
# Economic rights: Technische Universitaet Dresden (Germany)
#
# This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
#
# M3 is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# M3 is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License version 2 for more details.
#

#include <xtensa/config/specreg.h>
#include <xtensa/corebits.h>

#define FRAME_SIZE      132

.macro save_state
    addi    a3, a1, -FRAME_SIZE                 // we put the registers on the user stack
    s32i    a0, a3, 0                           // save a0 to 15 to a3; save a3 later
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
    rsr     a2, EXCSAVE_1                       // restore a3
    s32i    a2, a3, 12                          // save it
    rsr     a2, LBEG                            // save LBEG (we might want to use loops as well)
    s32i    a2, a3, 64
    rsr     a2, LEND                            // save LEND
    s32i    a2, a3, 68
    rsr     a2, LCOUNT                          // save LCOUNT
    s32i    a2, a3, 72
    rsr     a2, WINDOWBASE                      // save WINDOWBASE
    s32i    a2, a3, 76
    rsr     a2, WINDOWSTART                     // save WINDOWSTART
    s32i    a2, a3, 80
    rsr     a2, SAR                             // save SAR
    s32i    a2, a3, 84
    rsr     a2, PS                              // save PS
    s32i    a2, a3, 88
    rsr     a2, EPC_1                           // save PC where the user got interrupted
    s32i    a2, a3, 92
    movi    a2, 0                               // ensure that LCOUNT is zero; otherwise it causes
    wsr     a2, LCOUNT                          // trouble when the PC reaches LEND
    isync
    mov     a1, a3
.endm


    .extern ExceptionHandler
    .weak SyscallHandler
    .global _Exception

    .section .UserExceptionVector.text, "ax"
    .begin literal_prefix .UserExceptionVector
    .align  4

// interrupts & exceptions arrive here if PS.UM=1 and PS.EXCM=0
_UserExceptionVector:
    wsr     a3, EXCSAVE_1                       // save a3 to be able to use it
    movi    a3, _Exception                      // go to exception handler, we don't have enough
    jx      a3                                  // space here

    .size   _UserExceptionVector, .-_UserExceptionVector

    .end literal_prefix

    .section .text
_Exception:
    movi    a3, PS_WOE_MASK + 1                 // PS.INTLEVEL=1, PS.EXCM=0, PS.UM=0, PS.RING=0,
    wsr     a3, PS                              // PS.WOE=1; PS.OWB and PS.CALLINC are unused
    rsync
    save_state
    rsr     a6, EXCCAUSE
    mov     a7, a3
    movi    a4, ExceptionHandler
    callx4  a4
    // stay here
1:
    j       1b

    .size   _Exception, .-_Exception
