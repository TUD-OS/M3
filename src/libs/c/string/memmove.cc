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

void *memmove(void *dest, const void *src, size_t count) {
    /* nothing to do? */
    if(reinterpret_cast<uint8_t*>(dest) == reinterpret_cast<const uint8_t*>(src))
        return dest;

    /* moving forward */
    if(reinterpret_cast<uint8_t*>(dest) > reinterpret_cast<const uint8_t*>(src)) {
        const uint8_t *s = reinterpret_cast<const uint8_t*>(src) + count - 1;
        uint8_t *d = reinterpret_cast<uint8_t*>(dest) + count - 1;
        while(count-- > 0)
            *d-- = *s--;
    }
    /* moving backwards */
    else
        memcpy(dest,src,count);

    return dest;
}
