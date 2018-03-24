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

#include <base/KIF.h>

#include <fs/internal.h>

#include "Allocator.h"
#include "../FSHandle.h"

/**
 * Walks over each block of the inode, assigning it to <bno>.
 */
#define foreach_block(h, inode, bno)                                                    \
    blockno_t bno;                                                                      \
    Extent *__ch, *__indir = nullptr;                                                   \
    for(uint32_t __j, __i = 0; __i < (inode)->extents; ++__i)                           \
        for(__ch = INodes::get_extent((h), (inode), __i, &__indir, false), __j = 0;     \
            (bno = __ch->start + __j) && __j < __ch->length; ++__j)

class INodes {
    INodes() = delete;

public:
    static m3::INode *create(FSHandle &h, m3::mode_t mode);
    static void free(FSHandle &h, m3::inodeno_t ino);

    static m3::INode *get(FSHandle &h, m3::inodeno_t ino);

    static void stat(FSHandle &h, const m3::INode *inode, m3::FileInfo &info);

    static size_t seek(FSHandle &h, m3::INode *inode, size_t &off, int whence,
                       size_t &extent, size_t &extoff);

    static size_t get_extent_mem(FSHandle &h, m3::INode *inode, size_t extent,
                                 int perms, capsel_t sel);
    static size_t req_append(FSHandle &h, m3::INode *inode, size_t i, capsel_t sel,
                             int perm, m3::Extent *ext);
    static m3::Errors::Code append_extent(FSHandle &h, m3::INode *inode, m3::Extent *next);

    static m3::Extent *get_extent(FSHandle &h, m3::INode *inode, size_t i,
                                  m3::Extent **indir, bool create);
    static m3::Extent *change_extent(FSHandle &h, m3::INode *inode, size_t i,
                                     m3::Extent **indir, bool remove);
    static void fill_extent(FSHandle &h, m3::INode *inode, m3::Extent *ext, uint32_t blocks);

    static void truncate(FSHandle &h, m3::INode *inode, size_t extent, size_t extoff);

    static void mark_dirty(FSHandle &h, m3::inodeno_t ino);
    static void write_back(FSHandle &h, m3::INode *inode);
};