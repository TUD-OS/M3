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

_start:
    # set the location to jump to on exit
    mov     $BOOT_EXIT, %rax
    movq    $_start, (%rax)

1:
    # check if there is something to run
    mov     $BOOT_ENTRY, %rax
    mov     (%rax), %rcx
    test    %rcx, %rcx
    jne     1f

    # wait until we get waked up
    hlt
    jmp     1b

1:
    # jump to entry point
    jmp     *%rcx

    # just to be sure
    hlt
