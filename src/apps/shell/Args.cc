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

#include <m3/vfs/Dir.h>
#include <cstring>

#include "Args.h"

using namespace m3;

int Args::strmatch(const char *pattern, const char *str) {
    const char *lastStar;
    char *firstStar = (char*)strchr(pattern, '*');
    if(firstStar == NULL)
        return strcmp(pattern, str) == 0;
    lastStar = strrchr(pattern, '*');
    /* does the beginning match? */
    if(firstStar != pattern) {
        if(strncmp(str, pattern, static_cast<size_t>(firstStar - pattern)) != 0)
            return false;
    }
    /* does the end match? */
    if(lastStar[1] != '\0') {
        size_t plen = strlen(pattern);
        size_t slen = strlen(str);
        size_t cmplen = static_cast<size_t>(pattern + plen - lastStar - 1);
        if(strncmp(lastStar + 1, str + slen - cmplen, cmplen) != 0)
            return false;
    }

    /* now check whether the parts between the stars match */
    str += firstStar - pattern;
    while(1) {
        const char *match;
        const char *start = firstStar + 1;
        firstStar = (char*)strchr(start, '*');
        if(firstStar == NULL)
            break;

        *firstStar = '\0';
        match = strstr(str, start);
        *firstStar = '*';
        if(match == NULL)
            return false;
        str = match + (firstStar - start);
    }
    return true;
}

void Args::glob(ArgList *list, size_t i) {
    char filepat[MAX_ARG_LEN];
    char *pat = const_cast<char*>(list->args[i]);
    char *slash = strrchr(pat, '/');
    char old = '\0';
    if(slash) {
        strcpy(filepat, slash + 1);
        old = slash[1];
        slash[1] = '\0';
    }
    else
        strcpy(filepat, pat);
    size_t patlen = strlen(pat);

    Dir dir(pat);
    Dir::Entry e;
    bool found = false;
    while(dir.readdir(e)) {
        if(strcmp(e.name, ".") == 0 || strcmp(e.name, "..") == 0)
            continue;

        if(strmatch(filepat, e.name)) {
            if(patlen + strlen(e.name) + 1 <= MAX_ARG_LEN) {
                if(found) {
                    // move the following args forward
                    for(size_t x = list->count - 1; x >= i; --x)
                        list->args[x + 1] = list->args[x];
                    list->count++;
                }
                else
                    Heap::free((void*)list->args[i]);

                list->args[i] = (char*)Heap::alloc(patlen + strlen(e.name) + 1);
                strcpy((char*)list->args[i], pat);
                strcpy((char*)list->args[i] + patlen, e.name);
                i++;
                found = true;
                if(list->count >= ARRAY_SIZE(list->args))
                    break;
            }
        }
    }

    if(!found) {
        if(slash)
            slash[1] = old;

        // remove wildcard argument
        Heap::free((void*)list->args[i]);
        for(size_t x = i; x < list->count - 1; ++x)
            list->args[x] = list->args[x + 1];
        list->count--;
    }
}

void Args::prefix_path(ArgList *args) {
    if(args->args[0][0] != '/') {
        size_t len = strlen(args->args[0]);
        char *newstr = (char*)Heap::alloc(len + 5 + 1);
        strcpy(newstr, "/bin/");
        strcpy(newstr + 5, args->args[0]);
        Heap::free((void*)args->args[0]);
        args->args[0] = newstr;
    }
}

void Args::expand(ArgList *list) {
    for(size_t i = 0; i < list->count; ++i) {
        if(strchr(list->args[i], '*'))
            glob(list, i);
    }
}
