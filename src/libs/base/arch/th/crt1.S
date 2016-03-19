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

#if defined(__t2__)
#   include <base/arch/t2/Config.h>
#else
#   include <base/arch/t3/Config.h>
#endif

    .text
    .literal_position

    .literal LIT_STACK_TOP, STACK_TOP
    # user vector mode and windowed overflow detection
    .literal LIT_PS_VALUE, 0x00040020

    .extern  env_run

    .align  4
    .global _start
    .type   _start, @function

_start:
    # set return-addr and stack-pointer
    movi    a0, 0

    l32r    a1, LIT_STACK_TOP
    # use default stack pointer?
    addi.n  a1, a1, -4
    l32i    a2, a1, 0
    movi    a3, 0xDEADBEEF
    addi.n  a1, a1, -4
    bne     a2, a3, 1f
    # load SP from next word
    l32i    a1, a1, 0
1:

    # set PS
    l32r    a3, LIT_PS_VALUE
    wsr.ps  a3
    rsync

    call4   env_run

    # just to be sure
1:
    j       1b
    .size   _start, .-_start
