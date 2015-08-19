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

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <err.h>

int main(int argc, char **argv) {
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <file> <addr> [--sim]\n", argv[0]);
        return EXIT_FAILURE;
    }

    bool sim = argc > 3 && strcmp(argv[3], "--sim") == 0;

    FILE *f = fopen(argv[1], "r");
    if(!f)
        err(1, "fopen: %s\n", strerror(errno));

    if(!sim)
        printf("@%08lx\n", strtoul(argv[2], nullptr, 0) / 8);
    else
        printf("@%#08lx\n", strtoul(argv[2], nullptr, 0));

    while(!feof(f)) {
        ssize_t i;
        uint8_t buf[8];
        for(i = 0; i < 8; ++i) {
            buf[i] = fgetc(f);
            if(feof(f))
                break;
        }
        for(; i < 8; ++i)
            buf[i] = 0;
        if(sim) {
            for(i = 0; i < 8; ++i)
                printf("0x%02x ",buf[i]);
            printf("\n");
        }
        else {
            for(i = 7; i >= 0; --i)
                printf("%02x",buf[i]);
            printf("\n");
        }
    }

    fclose(f);
    return EXIT_SUCCESS;
}
