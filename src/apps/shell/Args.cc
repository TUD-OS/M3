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

#include <m3/vfs/Dir.h>
#include <cstring>

#include "Args.h"

using namespace m3;

char Args::argvals[MAX_ARG_COUNT][MAX_ARG_LEN];

int Args::strmatch(const char *pattern, const char *str) {
    const char *lastStar;
    char *firstStar = (char*)strchr(pattern, '*');
    if(firstStar == NULL)
        return strcmp(pattern, str) == 0;
    lastStar = strrchr(pattern, '*');
    /* does the beginning match? */
    if(firstStar != pattern) {
        if(strncmp(str, pattern, firstStar - pattern) != 0)
            return false;
    }
    /* does the end match? */
    if(lastStar[1] != '\0') {
        size_t plen = strlen(pattern);
        size_t slen = strlen(str);
        size_t cmplen = pattern + plen - lastStar - 1;
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

void Args::glob(char **args, size_t *i) {
    char filepat[MAX_ARG_LEN];
    char *pat = args[*i];
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
                strcpy(argvals[*i], pat);
                strcpy(argvals[*i] + patlen, e.name);
                args[*i] = argvals[*i];
                (*i)++;
                found = true;
                if(*i + 1 >= MAX_ARG_COUNT)
                    break;
            }
        }
    }

    if(!found) {
        if(slash)
            slash[1] = old;
        (*i)++;
    }
}

char **Args::parse(const char *line, int *argc) {
    static char *args[MAX_ARG_COUNT];
    size_t i = 0,j = 0;
    args[0] = argvals[0];
    while(*line) {
        if(Chars::isspace(*line)) {
            if(args[j][0]) {
                if(j + 2 >= MAX_ARG_COUNT)
                    break;
                args[j][i] = '\0';
                if(strchr(args[j], '*'))
                    glob(args, &j);
                else
                    j++;
                i = 0;
                args[j] = argvals[j];
            }
        }
        else if(i < MAX_ARG_LEN)
            args[j][i++] = *line;
        line++;
    }
    args[j][i] = '\0';
    if(strchr(args[j], '*')) {
        glob(args, &j);
        j--;
    }
    args[j + 1] = NULL;
    *argc = j + 1;

    return args;
}
