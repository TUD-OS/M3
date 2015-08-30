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

    # wait until a program has been loaded
    # TODO later, we should go into a low power mode here
    mov     $BOOT_ENTRY, %rax
1:
    mov     (%rax), %rcx
    test    %rcx, %rcx
    je      1b

    # jump to entry point
    jmp     *%rcx

    # just to be sure
    hlt
