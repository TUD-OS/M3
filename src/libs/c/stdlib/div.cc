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

long long divide(long long n, long long d, long long *rem) {
    long lrem;
    long res = divide((long)n, (long)d, &lrem);
    *rem = lrem;
    return res;
}

long divide(long n, long d, long *rem) {
    if(d == 0)
        return 0;

    long q = 0;
    long r = 0;
    for(int i = sizeof(long) * 8 - 1; i >= 0; --i) {
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
