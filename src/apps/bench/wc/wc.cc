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

#include <base/util/Profile.h>
#include <base/util/Chars.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/FileRef.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[4096];

NOINLINE void count(const char *buffer, long res, long *lines, long *words, int *last_space) {
    long i;
    for(i = 0; i < res; ++i) {
        if(buffer[i] == '\n')
            (*lines)++;
        int space = Chars::isspace(buffer[i]);
        if(!*last_space && space)
            (*words)++;
        *last_space = space;
    }
}

int main(int argc, char **argv) {
    if(argc != 2)
        exitmsg("Usage: " << argv[0] << " <file>");

    FileRef rf(argv[1], FILE_R);
    if(Errors::occurred())
        exitmsg("open of " << argv[1] << " failed");

    cycles_t start = Profile::start(0);
    long lines = 0;
    long words = 0;
    long bytes = 0;

    ssize_t res;
    int last_space = false;
    while((res = rf->read(buffer, sizeof(buffer))) > 0) {
        count(buffer, res, &lines, &words, &last_space);
        bytes += res;
    }
    cycles_t end = Profile::stop(0);

    cout << fmt(lines, 7) << " " << fmt(words, 7) << " " << fmt(bytes, 7) << "\n";
    cout << "Total time: " << (end - start) << "\n";
    return 0;
}
