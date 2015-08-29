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

#include <m3/Config.h>

.global _start
.extern main
.extern exit
.extern __clibrary_init

_start:
    mov     $stack, %esp
    mov     $0, %ebp

    # __clibrary_init(argc, argv)
    mov     $ARGC_ADDR, %rdi
    mov     (%rdi), %rdi
    mov     $ARGV_ADDR, %rsi
    mov     (%rsi), %rsi
    call    __clibrary_init

    # res = main(argc, argv)
    mov     $ARGC_ADDR, %rdi
    mov     (%rdi), %rdi
    mov     $ARGV_ADDR, %rsi
    mov     (%rsi), %rsi
    call    main

    # exit(res)
    mov     %rax, %rdi
    call    exit

    # just to be sure
    hlt

.section .data
.align 4096
.rept 4096
    .byte 0
.endr
stack:
