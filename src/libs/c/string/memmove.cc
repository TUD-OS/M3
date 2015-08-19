/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

void *memmove(void *dest, const void *src, size_t count) {
    uchar *s,*d;
    /* nothing to do? */
    if((uchar*)dest == (uchar*)src)
        return dest;

    /* moving forward */
    if((uchar*)dest > (uchar*)src) {
        s = (uchar*)src + count - sizeof(uchar);
        d = (uchar*)dest + count - sizeof(uchar);
        while(count-- > 0)
            *d-- = *s--;
    }
    /* moving backwards */
    else
        memcpy(dest,src,count);

    return dest;
}
