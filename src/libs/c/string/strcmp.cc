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

int strcmp(const char *str1, const char *str2) {
    char c1 = *str1, c2 = *str2;
    while(c1 && c2) {
        /* different? */
        if(c1 != c2) {
            if(c1 > c2)
                return 1;
            return -1;
        }
        c1 = *++str1;
        c2 = *++str2;
    }
    /* check which strings are finished */
    if(!c1 && !c2)
        return 0;
    if(!c1 && c2)
        return -1;
    return 1;
}
