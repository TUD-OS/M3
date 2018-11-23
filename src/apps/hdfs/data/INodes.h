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
#define foreach_extent(h, inode, ext, used_blocks)                                                 \
    Extent *__indir = nullptr;                                                                     \
    Extent *ext = INodes::get_extent((h), (inode), 0, &__indir, false, (used_blocks));             \
    for(uint32_t __i = 0;                                                                          \
        __i < (inode)->extents;                                                                    \
        ++__i, ext = INodes::get_extent((h), (inode), __i, &__indir, false, (used_blocks)))

/**
 * Walks over each block of the inode, assigning it to <bno>.
 */
#define foreach_block(h, ext, bno)                                                                 \
    blockno_t bno;                                                                                 \
    for(uint32_t __j = 0; (bno = ext->start + __j) && __j < ext->length; ++__j)

class INodes {
    INodes() = delete;

public:
    static m3::INode *create(FSHandle &h, m3::mode_t mode, UsedBlocks *used_blocks);
    static void free(FSHandle &h, m3::inodeno_t ino, UsedBlocks *used_blocks = nullptr);

    static m3::INode *get(FSHandle &h, m3::inodeno_t ino, UsedBlocks *used_blocks);

    static void stat(FSHandle &h, const m3::INode *inode, m3::FileInfo &info);

    static size_t seek(FSHandle &h, m3::INode *inode, size_t &off, int whence, size_t &extent,
                       size_t &extoff, UsedBlocks *used_blocks);

    static size_t get_extent_mem(FSHandle &h, m3::INode *inode, size_t extent, size_t extoff,
                                 size_t *extlen, int perms, capsel_t sel, bool dirty,
                                 UsedBlocks *used_blocks, size_t accessed);
    static size_t req_append(FSHandle &h, m3::INode *inode, size_t i, size_t extoff, size_t *extlen,
                             capsel_t sel, int perm, m3::Extent *ext, UsedBlocks *used_blocks,
                             size_t accessed);
    static m3::Errors::Code append_extent(FSHandle &h, m3::INode *inode, m3::Extent *next,
                                          size_t *prev_ext_len, UsedBlocks *used_blocks);

    static m3::Extent *get_extent(FSHandle &h, m3::INode *inode, size_t i, m3::Extent **indir,
                                  bool create, UsedBlocks *used_blocks);
    static m3::Extent *change_extent(FSHandle &h, m3::INode *inode, size_t i, m3::Extent **indir,
                                     bool remove, UsedBlocks *used_blocks);
    static void fill_extent(FSHandle &h, m3::INode *inode, m3::Extent *ext, uint32_t blocks,
                            size_t accessed, UsedBlocks *used_blocks);

    static void truncate(FSHandle &h, m3::INode *inode, size_t extent, size_t extoff,
                         UsedBlocks *used_blocks);

    static void mark_dirty(FSHandle &h, m3::inodeno_t ino);
    static void write_back(FSHandle &h, m3::INode *inode, UsedBlocks *used_blocks);
};
