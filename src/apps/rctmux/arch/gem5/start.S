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

.global _start
.global _rctmux_stack
.extern _try_run

.text

_start:
    # enable interrupts
    sti
    # use a small stack
    mov     $_rctmux_stack, %rsp
    mov     %rsp, %rbp

    # check if there is something to run
    call    _try_run

    # _if we get here, there was none, so idle
    hlt
    jmp     _start

.data

    .align 16
    .space 512, 0
_rctmux_stack:
