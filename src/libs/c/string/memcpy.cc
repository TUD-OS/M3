/*
 * Copyright (C) 2015-2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
    uint8_t *bdest = reinterpret_cast<uint8_t*>(dest);
    const uint8_t *bsrc = reinterpret_cast<const uint8_t*>(src);

    /* are both aligned equally? */
    size_t dalign = reinterpret_cast<uintptr_t>(bdest) % sizeof(word_t);
    size_t salign = reinterpret_cast<uintptr_t>(bsrc) % sizeof(word_t);
    if(dalign == salign) {
        // align them to a word-boundary
        while(len > 0 && reinterpret_cast<uintptr_t>(bdest) % sizeof(word_t)) {
            *bdest++ = *bsrc++;
            len--;
        }

        word_t *ddest = reinterpret_cast<word_t*>(bdest);
        const word_t *dsrc = reinterpret_cast<const word_t*>(bsrc);
        /* copy words with loop-unrolling */
        while(len >= sizeof(word_t) * 8) {
            ddest[0] = dsrc[0];
            ddest[1] = dsrc[1];
            ddest[2] = dsrc[2];
            ddest[3] = dsrc[3];
            ddest[4] = dsrc[4];
            ddest[5] = dsrc[5];
            ddest[6] = dsrc[6];
            ddest[7] = dsrc[7];
            ddest += 8;
            dsrc += 8;
            len -= sizeof(word_t) * 8;
        }

        /* copy remaining words */
        while(len >= sizeof(word_t)) {
            *ddest++ = *dsrc++;
            len -= sizeof(word_t);
        }

        bdest = reinterpret_cast<uint8_t*>(ddest);
        bsrc = reinterpret_cast<const uint8_t*>(dsrc);
    }

    /* copy remaining bytes */
    while(len-- > 0)
        *bdest++ = *bsrc++;
    return dest;
}
