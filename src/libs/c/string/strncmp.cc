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

int strncmp(const char *str1,const char *str2,size_t count) {
    ssize_t rem = count;
    while(*str1 && *str2 && rem-- > 0) {
        if(*str1++ != *str2++)
            return str1[-1] < str2[-1] ? -1 : 1;
    }
    if(rem <= 0)
        return 0;
    if(*str1 && !*str2)
        return 1;
    return -1;
}
