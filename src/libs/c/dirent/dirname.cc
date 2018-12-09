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

#include <libgen.h>
#include <string.h>

char *dirname(char *path) {
    static char dot[] = ".";
    if(!path || !path[0])
        return dot;
    if((path[0] == '.' || path[0] == '/') && path[1] == '\0')
        return path;
    if(path[0] == '.' && path[1] == '.' && path[2] == '\0') {
        path[1] = '\0';
        return path;
    }

    size_t len = strlen(path);
    while(len > 0 && path[len - 1] == '/')
        len--;
    while(len > 0 && path[len - 1] != '/')
        len--;
    while(len > 0 && path[len - 1] == '/')
        len--;
    if(path[len] == '/')
        path[len > 0 ? len : 1] = '\0';
    else {
        path[0] = '.';
        path[1] = '\0';
    }
    return path;
}
