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

.global _vector_table
.global _start
.global _exc_entry
.extern exc_handler

.section .vectors

// will be filled at runtime
_vector_table:
.rept   1023
    nop
.endr

.text

_start:
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
    ldr     r0, =_rctmux_stack
    mov     sp, r0
    mov     fp, #0

    // check if have an application
    bl      _start_app
    cmp     r0, #0
    bne     _restore

    // set idle stack
    ldr     r0, =_idle_stack
    mov     sp, r0

    // enable interrupts
    mrs     r0, CPSR
    bic     r0, #1 << 7
    msr     CPSR, r0

    // wait until we get waked up
    bl      _sleep

    // try again
    b       _start

_restore:
    mov     sp, r0
    // restore CPSR
    pop     {r0}
    msr     CPSR, r0
    // restore register
    pop     {r0-r12, lr}
    // continue execution
    pop     {pc}

_exc_entry:
    // repeat last instruction
    sub     lr, lr, #4

    // temporarily save lr (old pc) and SPSR (old CPSR)
    ldr     sp, =_tmp
    str     lr, [sp]
    mrs     lr, SPSR
    str     lr, [sp, #4]

    // switch back to supervisor mode
    mrs     sp, CPSR
    bic     sp, #0x1F
    orr     sp, sp, #0x13
    msr     CPSR, sp

    // leave one slot for the pc
    sub     sp, sp, #4

    // save regs
    push    {r0-r12, lr}

    // save pc
    ldr     r0, =_tmp
    ldr     r1, [r0]
    str     r1, [sp, #14 * 4]

    // save cpsr
    ldr     r1, [r0, #4]
    push    {r1}

    // call handler
    mov     r0, sp
    bl      exc_handler

    // the kernel will reset the PE
1:  mrs     r0, CPSR
    bic     r0, #1 << 7
    msr     CPSR, r0
    // wait for interrupt
    wfi
    b       1b

.bss
_tmp:
    .long       0
    .long       0

    .align      2
    .space 512, 0
_rctmux_stack:

    .space 512, 0
_idle_stack:
