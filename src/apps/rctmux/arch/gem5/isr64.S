#
# Copyright (C) 2016, René Küttner <rene.kuettner@tu-dresden.de>
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

.extern _interrupt_handler
.global _rctmux_stack

.global isr64
isr64:
    push    $0                      // error-code (no error here)
    push    $64                     // the interrupt-number

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

    // switch to rctmux' stack, store current stack pointer and
    // call C routine; read stack pointer from rctmux' stack back
    // after returning from C
    mov     %rsp, %rax
    mov     $_rctmux_stack, %rsp
    push    %rax
    pushf
    cld
    call    _interrupt_handler
    popf
    pop     %rsp

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
