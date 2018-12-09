/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#pragma once

#include <fs/internal.h>

#include "../sess/Request.h"
#include "../FSHandle.h"

/**
 * Walks over all dir-entries in block <bno>, assigning it to <e>.
 */
#define foreach_direntry(r, bno, e)                                                             \
    DirEntry *e = reinterpret_cast<DirEntry*>((r).hdl().metabuffer().get_block((r), (bno)));    \
    DirEntry *__eend = e + (r).hdl().sb().blocksize / sizeof(DirEntry);                         \
    for(; e < __eend; e = reinterpret_cast<DirEntry*>(reinterpret_cast<char*>(e) + e->next))

class Dirs {
    Dirs() = delete;

    static m3::DirEntry *find_entry(Request &r, m3::INode *inode, const char *name, size_t namelen);

public:
    static m3::inodeno_t search(Request &r, const char *path, bool create = false);
    static m3::Errors::Code create(Request &r, const char *path, m3::mode_t mode);
    static m3::Errors::Code remove(Request &r, const char *path);
    static m3::Errors::Code link(Request &r, const char *oldpath, const char *newpath);
    static m3::Errors::Code unlink(Request &r, const char *path, bool isdir);
};
