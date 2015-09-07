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

#include <m3/stream/FStream.h>
#include <m3/service/M3FS.h>
#include <m3/vfs/VFS.h>
#include <m3/Log.h>

using namespace m3;

int main(int argc, char **argv) {
    if(argc < 2) {
        Serial::get() << "Usage: " << argv[0] << " <file>...\n";
        return 1;
    }

    if(VFS::mount("/", new M3FS("m3fs")) < 0) {
        if(Errors::last != Errors::EXISTS)
            PANIC("Mounting root-fs failed");
    }

    size_t bufsize = 128;
    char *buffer = (char*)Heap::alloc(bufsize);

    for(int i = 1; i < argc; ++i) {
        FStream input(argv[i], FILE_R);
        if(Errors::occurred()) {
            Serial::get() << "Open of " << argv[i] << " failed (" << Errors::last << ")\n";
            continue;
        }

        while(!input.eof()) {
            input.getline(buffer, bufsize);
            Serial::get() << buffer << "\n";
        }
    }
    return 0;
}
