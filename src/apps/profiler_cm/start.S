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

#include <m3/arch/t2/Config.h>

    .text
    .literal_position

    .literal LIT_PS, 0x00040020
    .literal LIT_STACK, __stack

    .extern  main

    .align  4
    .global _start
    .type   _start, @function

_start:
    # set return-addr and stack-pointer
    movi    a0, 0
    l32r    a1, LIT_STACK
    # set PS
    l32r    a3, LIT_PS
    wsr.ps  a3
    rsync

    call4   main

    # not reached
1:  j       1b

    .size   _start, .-_start
