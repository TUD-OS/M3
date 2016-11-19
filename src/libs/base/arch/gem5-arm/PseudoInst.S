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

.global gem5_shutdown
.global gem5_writefile
.global gem5_readfile

gem5_shutdown:
    .long   0xEE210110

1:  wfi
    b       1b

gem5_writefile:
    .long   0xEE4F0110
    mov     pc, lr

gem5_readfile:
    .long   0xEE500110
    mov     pc, lr
