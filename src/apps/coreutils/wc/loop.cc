/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include "loop.h"

static inline int isblank(int c) {
    return c == ' ' || c == '\t';
}

static inline int isspace(int c) {
    return isblank(c) || c == '\v' || c == '\f' || c == '\r' || c == '\n';
}

void count(const char *buffer, size_t res, long *lines, long *words, int *last_space) {
    for(size_t i = 0; i < res; ++i) {
        if(buffer[i] == '\n')
            (*lines)++;
        int space = isspace(buffer[i]);
        if(!*last_space && space)
            (*words)++;
        *last_space = space;
    }
}
