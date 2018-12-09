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

void *memset(void *addr, int value, size_t count) {
    uint8_t *baddr = reinterpret_cast<uint8_t*>(addr);
    /* align it */
    while(count > 0 && reinterpret_cast<uintptr_t>(baddr) % sizeof(ulong)) {
        *baddr++ = value;
        count--;
    }

    uint byte = static_cast<uint8_t>(value);
    word_t dwval = (byte << 24) | (byte << 16) | (byte << 8) | byte;
    if(sizeof(word_t) == 8)
        dwval |= static_cast<uint64_t>(dwval) << 32;

    word_t *waddr = reinterpret_cast<word_t*>(baddr);
    /* set words with loop-unrolling */
    while(count >= sizeof(word_t) * 8) {
        waddr[0] = dwval;
        waddr[1] = dwval;
        waddr[2] = dwval;
        waddr[3] = dwval;
        waddr[4] = dwval;
        waddr[5] = dwval;
        waddr[6] = dwval;
        waddr[7] = dwval;
        waddr += 8;
        count -= sizeof(word_t) * 8;
    }

    /* set with dwords */
    while(count >= sizeof(word_t)) {
        *waddr++ = dwval;
        count -= sizeof(word_t);
    }

    /* set remaining bytes */
    baddr = reinterpret_cast<uint8_t*>(waddr);
    while(count-- > 0)
        *baddr++ = value;
    return addr;
}
