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

#include <c/div.h>

template<typename T>
T divmod(T n, T d, T *rem) {
    if(d == 0)
        return 0;

    T q = 0;
    T r = 0;
    for(int i = sizeof(T) * 8 - 1; i >= 0; --i) {
        r <<= 1;
        r |= (n >> i) & 0x1;
        if(r >= d) {
            r = r - d;
            q |= 1 << i;
        }
    }
    *rem = r;
    return q;
}

long divide(long n, long d, long *rem) {
    return divmod(n, d, rem);
}

llong divide(llong n, llong d, llong *rem) {
    return divmod(n, d, rem);
}

// we provide our own versions of __moddi3, __divdi3, __umoddi3 and __udivdi3 to optimize for size
// instead of performance. they are almost exclusively used for printing stuff anyway.

extern "C" llong __moddi3(llong n, llong d) {
    llong rem = 0;
    divmod(n, d, &rem);
    return rem;
}

extern "C" llong __divdi3(llong n, llong d) {
    llong rem;
    return divmod(n, d, &rem);
}

extern "C" ullong __umoddi3(ullong n, ullong d) {
    ullong rem = 0;
    divmod(n, d, &rem);
    return rem;
}

extern "C" ullong __udivdi3(ullong n, ullong d) {
    ullong rem;
    return divmod(n, d, &rem);
}
