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

#if defined(__t2__)
#   include <m3/arch/t2/Config.h>
#else
#   include <m3/arch/t3/Config.h>
#endif

    .text
    .literal_position

    .literal LIT_STACK_TOP, STACK_TOP
    .literal LIT_PS, 0x00040020
    .literal LIT_BOOT_ENTRY, BOOT_ENTRY
    .literal LIT_BOOT_EXIT, BOOT_EXIT

    .extern  setup_irq
    .extern  run_task
    .extern  print

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

    # let the program know where to jump to when finished
    l32r    a4, LIT_BOOT_EXIT
    # start over to reset the stack-pointer etc.
    movi.n  a3, _start
    s32i.n  a3, a4, 0

    # setup interrupts
    call4   setup_irq

1:
    # wait for an interrupt
    # TODO is broken on the FFT core
#if !defined(__t3__)
    waiti   0
#endif

    # is there a callback?
    l32r    a4, LIT_BOOT_ENTRY
    l32i.n  a3, a4, 0
    # _if its zero, there is nothing to call
    beqz.n  a3, 2f
    jx      a3

2:
    # check if there is a task to run
#if defined(__t2__)
    call4   run_task
#endif
    j       1b

    # not reached
1:  j       1b

    .size   _start, .-_start
