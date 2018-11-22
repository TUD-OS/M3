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

#pragma once

#include <fs/internal.h>

#include "../FSHandle.h"
#include "../sess/MetaSession.h"

/**
 * Walks over all dir-entries in block <bno>, assigning it to <e>.
 */
#define foreach_direntry(h, bno, e)                                                     \
    DirEntry *e = reinterpret_cast<DirEntry*>((h).metabuffer().get_block((bno)));     \
    DirEntry *__eend = e + (h).sb().blocksize / sizeof(DirEntry);                       \
    for(; e < __eend; e = reinterpret_cast<DirEntry*>(reinterpret_cast<char*>(e) + e->next))

class Dirs {
    Dirs() = delete;

public:
    static m3::DirEntry *find_entry(FSHandle &h, m3::INode *inode, const char *name, size_t namelen, UsedBlocks *used_blocks);
    static m3::inodeno_t search(FSHandle &h, const char *path, bool create = false, UsedBlocks *used_blocks = nullptr);
    static m3::Errors::Code create(FSHandle &h, const char *path, m3::mode_t mode, UsedBlocks *used_blocks);
    static m3::Errors::Code remove(FSHandle &h, const char *path, UsedBlocks *used_blocks);
    static m3::Errors::Code link(FSHandle &h, const char *oldpath, const char *newpath, UsedBlocks *used_blocks);
    static m3::Errors::Code unlink(FSHandle &h, const char *path, bool isdir, UsedBlocks *used_blocks);
};
