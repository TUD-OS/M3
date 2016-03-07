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
#   include <m3/arch/t2/Config.h>
#else
#   include <m3/arch/t3/Config.h>
#endif

    .text
    .literal_position

    .literal LIT_STACK_TOP, STACK_TOP
    .literal LIT_PS, 0x00040020

    .extern  setup_irq
    .extern  loop

    .align  4
    .global _start
    .type   _start, @function

_start:
    # set return-addr and stack-pointer
    movi    a0, 0
    l32r    a1, LIT_STACK_TOP
    # set PS
    l32r    a3, LIT_PS
    wsr.ps  a3
    rsync

    # setup interrupts
    call4   setup_irq

    call4   loop

    # not reached
1:  j       1b

    .size   _start, .-_start
