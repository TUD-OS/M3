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

namespace m3 {

bool Dir::readdir(Entry &e) {
    // read header
    DirEntry fse;
    if(_f.read(&fse, sizeof(fse)) != sizeof(fse))
        return false;

    // read name
    e.nodeno = fse.nodeno;
    if(_f.read(e.name, fse.namelen) != fse.namelen)
        return false;

    // 0-termination
    e.name[fse.namelen < Entry::MAX_NAME_LEN ? fse.namelen : Entry::MAX_NAME_LEN - 1] = '\0';

    // move to next entry
    size_t off = fse.next - (sizeof(fse) + fse.namelen);
    if(off != 0)
        _f.seek(off, SEEK_CUR);
    return true;
}

}
