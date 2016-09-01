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

.global _start

.extern _start_app
.extern interrupt_handler

.text

// put that at the beginning so that we know the address
.type   _start, @function
_start:
    // interrupts in rctmux are always disabled
    cli
    // use a small stack
    mov     $_rctmux_stack, %rsp
    mov     %rsp, %rbp

    call    _start_app

    // restore application state
    mov     %rax, %rsp
    pop     %r15
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %r11
    pop     %r10
    pop     %r9
    pop     %r8
    pop     %rbp
    pop     %rsi
    pop     %rdi
    pop     %rdx
    pop     %rcx
    pop     %rbx
    pop     %rax

    // remove error-code and interrupt-number from stack and return
    add     $16,%rsp
    iretq
.size   _start, .-_start

.type   isrCommon, @function
isrCommon:
    push    %rax
    push    %rbx
    push    %rcx
    push    %rdx
    push    %rdi
    push    %rsi
    push    %rbp
    push    %r8
    push    %r9
    push    %r10
    push    %r11
    push    %r12
    push    %r13
    push    %r14
    push    %r15

    // call C++ routine
    mov     %rsp, %rdi
    call    interrupt_handler

    // the kernel will reset the PE
    hlt

    // we should not get here
1:  jmp     1b

.size   isrCommon, .-isrCommon

// macro to build a default-isr-handler
.macro BUILD_DEF_ISR no
    .global isr\no
    .type   isr\no, @function
    isr\no:
    // interrupts are already disabled here since its a interrupt-gate, not a trap-gate
    push    $0                      // error-code (no error here)
    push    $\no                    // the interrupt-number
    jmp     isrCommon
    .size   isr\no, .-isr\no
.endm

// macro to build an error-isr-handler
.macro BUILD_ERR_ISR no
    .global isr\no
    .type   isr\no, @function
    isr\no:
    // interrupts are already disabled here since its a interrupt-gate, not a trap-gate
    // the error-code has already been pushed
    push    $\no                    // the interrupt-number
    jmp     isrCommon
    .size   isr\no, .-isr\no
.endm

// our ISRs
BUILD_DEF_ISR 0
BUILD_DEF_ISR 1
BUILD_DEF_ISR 2
BUILD_DEF_ISR 3
BUILD_DEF_ISR 4
BUILD_DEF_ISR 5
BUILD_DEF_ISR 6
BUILD_DEF_ISR 7
BUILD_ERR_ISR 8
BUILD_DEF_ISR 9
BUILD_ERR_ISR 10
BUILD_ERR_ISR 11
BUILD_ERR_ISR 12
BUILD_ERR_ISR 13
BUILD_ERR_ISR 14
BUILD_DEF_ISR 15
BUILD_DEF_ISR 16
BUILD_DEF_ISR 64
BUILD_DEF_ISR 65

.global isrNull
.type   isrNull, @function
isrNull:
    push    $0                      // error-code (no error here)
    push    $32                     // the interrupt-number
.size   isrNull, .-isrNull

.data

    .align 16
    .space 512, 0
_rctmux_stack:
