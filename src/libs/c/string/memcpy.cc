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

#include <base/Common.h>
#include <cstring>

/* this is necessary to prevent that gcc transforms a loop into library-calls
 * (which might lead to recursion here) */
#pragma GCC optimize ("no-tree-loop-distribute-patterns")

void *memcpy(void *dest, const void *src, size_t len) {
    uchar *bdest = (uchar*)dest;
    uchar *bsrc = (uchar*)src;

    /* are both aligned equally? */
    if(((uintptr_t)bdest % sizeof(ulong)) == ((uintptr_t)bsrc % sizeof(ulong))) {
        // align them to a word-boundary
        while(len > 0 && (uintptr_t)bdest % sizeof(ulong)) {
            *bdest++ = *bsrc++;
            len--;
        }

        ulong *ddest = (ulong*)bdest;
        ulong *dsrc = (ulong*)bsrc;
        /* copy words with loop-unrolling */
        while(len >= sizeof(ulong) * 8) {
            *ddest = *dsrc;
            *(ddest + 1) = *(dsrc + 1);
            *(ddest + 2) = *(dsrc + 2);
            *(ddest + 3) = *(dsrc + 3);
            *(ddest + 4) = *(dsrc + 4);
            *(ddest + 5) = *(dsrc + 5);
            *(ddest + 6) = *(dsrc + 6);
            *(ddest + 7) = *(dsrc + 7);
            ddest += 8;
            dsrc += 8;
            len -= sizeof(ulong) * 8;
        }

        /* copy remaining words */
        while(len >= sizeof(ulong)) {
            *ddest++ = *dsrc++;
            len -= sizeof(ulong);
        }

        bdest = (uchar*)ddest;
        bsrc = (uchar*)dsrc;
    }

    /* copy remaining bytes */
    while(len-- > 0)
        *bdest++ = *bsrc++;
    return dest;
}
