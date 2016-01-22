/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <m3/Common.h>
#include <cstring>

/* this is necessary to prevent that gcc transforms a loop into library-calls
 * (which might lead to recursion here) */
#pragma GCC optimize ("no-tree-loop-distribute-patterns")

void *memset(void *addr, int value, size_t count) {
    uchar *baddr = (uchar*)addr;
    /* align it */
    while(count > 0 && (uintptr_t)baddr % sizeof(ulong)) {
        *baddr++ = value;
        count--;
    }

    ulong dwval = (value << 24) | (value << 16) | (value << 8) | value;
    ulong *dwaddr = (ulong*)baddr;
    /* set words with loop-unrolling */
    while(count >= sizeof(ulong) * 8) {
        *dwaddr = dwval;
        *(dwaddr + 1) = dwval;
        *(dwaddr + 2) = dwval;
        *(dwaddr + 3) = dwval;
        *(dwaddr + 4) = dwval;
        *(dwaddr + 5) = dwval;
        *(dwaddr + 6) = dwval;
        *(dwaddr + 7) = dwval;
        dwaddr += 8;
        count -= sizeof(ulong) * 8;
    }

    /* set with dwords */
    while(count >= sizeof(ulong)) {
        *dwaddr++ = dwval;
        count -= sizeof(ulong);
    }

    /* set remaining bytes */
    baddr = (uchar*)dwaddr;
    while(count-- > 0)
        *baddr++ = value;
    return addr;
}
