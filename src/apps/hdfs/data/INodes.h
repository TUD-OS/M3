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

#include "../FSHandle.h"
#include "../sess/MetaSession.h"
#include "Allocator.h"

/**
 * Walks over each extent of the inode, assigning it to <ext>.
 */
#define foreach_extent(r, inode, ext)                                                              \
    Extent *__indir = nullptr;                                                                     \
    Extent *ext = INodes::get_extent((r), (inode), 0, &__indir, false);                            \
    for(uint32_t __i = 0;                                                                          \
        __i < (inode)->extents;                                                                    \
        ++__i, ext = INodes::get_extent((r), (inode), __i, &__indir, false))

/**
 * Walks over each block of the inode, assigning it to <bno>.
 */
#define foreach_block(ext, bno)                                                                    \
    blockno_t bno;                                                                                 \
    for(uint32_t __j = 0; (bno = ext->start + __j) && __j < ext->length; ++__j)

class INodes {
    INodes() = delete;

public:
    static m3::INode *create(Request &r, m3::mode_t mode);
    static void free(Request &r, m3::inodeno_t ino);

    static m3::INode *get(Request &r, m3::inodeno_t ino);

    static void stat(Request &r, const m3::INode *inode, m3::FileInfo &info);

    static size_t seek(Request &r, m3::INode *inode, size_t &off, int whence, size_t &extent,
                       size_t &extoff);

    static size_t get_extent_mem(Request &r, m3::INode *inode, size_t extent, size_t extoff,
                                 size_t *extlen, int perms, capsel_t sel, bool dirty, size_t accessed);
    static size_t req_append(Request &r, m3::INode *inode, size_t i, size_t extoff, size_t *extlen,
                             capsel_t sel, int perm, m3::Extent *ext, size_t accessed);
    static m3::Errors::Code append_extent(Request &r, m3::INode *inode, m3::Extent *next,
                                          size_t *prev_ext_len);

    static m3::Extent *get_extent(Request &r, m3::INode *inode, size_t i, m3::Extent **indir, bool create);
    static m3::Extent *change_extent(Request &r, m3::INode *inode, size_t i, m3::Extent **indir, bool remove);
    static void fill_extent(Request &r, m3::INode *inode, m3::Extent *ext, uint32_t blocks, size_t accessed);

    static void truncate(Request &r, m3::INode *inode, size_t extent, size_t extoff);

    static void mark_dirty(Request &r, m3::inodeno_t ino);
    static void write_back(Request &r, m3::INode *inode);
};
