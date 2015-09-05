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

#include <m3/pipe/PipeFS.h>
#include <m3/stream/IStringStream.h>
#include <m3/vfs/Pipe.h>

namespace m3 {

File *PipeFS::open(const char *path, int perms) {
    String spath(path);
    IStringStream is(spath);
    capsel_t caps;
    size_t rep, size;

    // form is (r|w)_<caps>_<rep>_<size>
    char type = is.read();
    is.read();
    is >> caps;
    is.read();
    is >> rep;
    is.read();
    is >> size;
    if(type == 'r') {
        if(perms != FILE_R) {
            Errors::last = Errors::INV_ARGS;
            return nullptr;
        }
        return new PipeFileReader(caps, rep);
    }

    // FStream enables FILE_R here, too
    if(~perms & FILE_W) {
        Errors::last = Errors::INV_ARGS;
        return nullptr;
    }
    return new PipeFileWriter(caps, size);
}

}
