#
# Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
# Economic rights: Technische Universitaet Dresden (Germany)
#
# This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <base/Asm.h>

.extern exc_handler
.extern init
.extern sleep
.global idle_stack

.text

BEGIN_FUNC(_start)
    // TODO currently required, because the reset lets us start at the next instruction
    nop

    mrc     p15, 0, r0, c10, c2, 0  // PRRR
    orr     r0, r0, #0x00000002     // set tr0 = 0x2 (normal memory)
    mcr     p15, 0, r0, c10, c2, 0

    mrc     p15, 0, r0, c10, c2, 1  // NMRR
    orr     r0, r0, #0x00000001     // set ir0 = 0x1 (write-back, write-allocate)
    orr     r0, r0, #0x00010000     // set or0 = 0x1 (write-back, write-allocate)
    mcr     p15, 0, r0, c10, c2, 1

    mrc     p15, 0, r0, c1, c0, 0   // SCTLR
    orr     r0, r0, #0x00001000     // enable I-cache
    orr     r0, r0, #0x00000004     // enable D-cache
    orr     r0, r0, #0x10000000     // enable TEX remap
    mcr     p15, 0, r0, c1, c0, 0

    // setup stack
    ldr     sp, =rctmux_stack
    mov     fp, #0

    // perform initialization, if necessary
    bl      init

    // enable interrupts and supervisor mode
    mov     r0, #0x13
    msr     CPSR, r0
    // start idling
    ldr     r0, =idle
    mov     pc, r0
END_FUNC(_start)

BEGIN_FUNC(idle)
1:  bl      sleep
    b       1b
END_FUNC(idle)

.macro BUILD_ENTRY, no
BEGIN_FUNC(entry_\no)
    // temporarily save lr (old pc)
    ldr     sp, =_tmp
    str     lr, [sp]
    // save the vector number
    mov     lr, #\no
    str     lr, [sp, #4]
    // save SPRS (old CPRS)
    mrs     lr, SPSR
    str     lr, [sp, #8]

    // switch back to supervisor mode
    mrs     sp, CPSR
    bic     sp, #0x1F
    orr     sp, sp, #0x13
    msr     CPSR, sp

.if \no == 6
    // use the application's stack; leave one slot for the pc
    sub     sp, sp, #4
.else
    // use our own stack
    ldr     sp, =rctmux_stack
.endif

    b       exc_common
END_FUNC(entry_\no)
.endm

BUILD_ENTRY 0
BUILD_ENTRY 1
BUILD_ENTRY 2
BUILD_ENTRY 3
BUILD_ENTRY 4
BUILD_ENTRY 6
BUILD_ENTRY 7

BEGIN_FUNC(exc_common)
    // save regs
    push    {r0-r12, lr}

    // save pc, cpsr and vector number
    ldr     r0, =_tmp
    ldm     r0, {r1, r2, r3}
    str     r1, [sp, #14 * 4]
    sub     sp, sp, #8
    stm     sp, {r2, r3}

    // call handler
    mov     r0, sp
    bl      exc_handler

    // skip vector number
    add     sp, r0, #4
    // restore CPSR
    pop     {r0}
    msr     CPSR, r0
    // restore register
    pop     {r0-r12, lr}
    // continue execution
    pop     {pc}
END_FUNC(exc_common)

.bss
_tmp:
    // temporary place for pc, cpsr and vector number
    .long       0
    .long       0
    .long       0

    .align      2
    .space 1024,0
rctmux_stack:
    .space 128, 0
idle_stack:
