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
#include <xtensa/simcall.h>

    .text
    .literal_position

    .literal LIT_STACK_TOP, STACK_TOP
    # user vector mode and windowed overflow detection
    .literal LIT_PS_VALUE, 0x00040020
    .literal LIT_BSS_START, _bss_table_start
    .literal LIT_BSS_END, _bss_table_end
    .literal LIT_ARGC_ADDR, ARGC_ADDR
    .literal LIT_ARGV_ADDR, ARGV_ADDR
    .literal LIT_BOOT_LAMBDA, BOOT_LAMBDA
    .literal LIT_BOOT_ENTRY, BOOT_ENTRY
    .literal LIT_BOOT_SP, BOOT_SP

    .extern   __clibrary_init
    .extern  main
    .extern  exit

    .align  4
    .global _start
    .type   _start, @function

_start:
    # set return-addr and stack-pointer
    movi    a0, 0
    # set stack-pointer; either from BOOT_SP or the default one
    l32r    a1, LIT_BOOT_SP
    l32i.n  a1, a1, 0
    bnez.n  a1, 1f
    l32r    a1, LIT_STACK_TOP
1:
    # set PS
    l32r    a3, LIT_PS_VALUE
    wsr.ps  a3
    rsync

#if defined(__t3__)
    # test whether we were started from M3 or from the simulator
    l32r    a3, LIT_BOOT_ENTRY
    l32i.n  a3, a3, 0
    bnez.n  a3, _m3_start

    # we need some space for the arguments
    movi    a2, SYS_iss_argv_size
    simcall
    addi.n  a2, a2, 15
    srli    a2, a2, 4
    slli    a2, a2, 4
    sub     a1, a1, a2

    # call memset(BSS_START, 0, BSS_END - BSS_START)
    l32r    a2, LIT_BSS_END
    l32r    a6, LIT_BSS_START
    movi.n  a7, 0
    sub     a8, a2, a6
    call4   memset

    # put argv here
    or      a3, a1, a1
    movi    a2, SYS_iss_set_argv
    simcall
    # get argc
    movi    a2, SYS_iss_argc
    simcall

    # call __clibrary_init(argc, argv)
    mov.n   a6, a2
    mov.n   a7, a1
    call4   __clibrary_init

    # call res = main(argc, argv)
    mov.n   a6, a2
    mov.n   a7, a1
    call4   main
    j       2f
#endif

_m3_start:
    # should we call main or a callback?
    l32r    a4, LIT_BOOT_LAMBDA
    l32i.n  a4, a4, 0
    beqi    a4, 0, 1f

    # init serial for ourself
    l32r    a3, LIT_ARGV_ADDR
    movi    a6, 0
    l32i.n  a7, a3, 0
    call4   __clibrary_init_lambda

    # call callback
    l32r    a6, LIT_BOOT_LAMBDA
    l32i.n  a6, a6, 0
    call4   lambda_main
    j       2f

    # call memset(BSS_START, 0, BSS_END - BSS_START)
1:
    l32r    a2, LIT_BSS_END
    l32r    a6, LIT_BSS_START
    movi.n  a7, 0
    sub     a8, a2, a6
    call4   memset

    # load argc and argv
    l32r    a2, LIT_ARGC_ADDR
    l32i.n  a2, a2, 0
    l32r    a3, LIT_ARGV_ADDR
    l32i.n  a3, a3, 0

    # call __clibrary_init(argc, argv)
    mov.n   a6, a2
    mov.n   a7, a3
    call4   __clibrary_init

    # call res = main(argc, argv)
    mov.n   a6, a2
    mov.n   a7, a3
    call4   main

2:
    # call exit(res)
    call4   exit

    .size   _start, .-_start
