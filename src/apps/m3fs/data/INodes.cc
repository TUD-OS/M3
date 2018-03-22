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

#include <base/util/Time.h>

#include <m3/Syscalls.h>
#include <m3/VPE.h>

#include "INodes.h"

using namespace m3;

alignas(64) static char zeros[MAX_BLOCK_SIZE];

INode *INodes::create(FSHandle &h, mode_t mode) {
    inodeno_t ino = h.inodes().alloc(h);
    if(ino == 0) {
        Errors::last = Errors::NO_SPACE;
        return nullptr;
    }
    INode *inode = get(h, ino);
    memset(inode, 0, sizeof(*inode));
    inode->inode = ino;
    inode->devno = 0; /* TODO */
    inode->mode = mode;
    mark_dirty(h, ino);
    return inode;
}

void INodes::free(FSHandle &h, inodeno_t ino) {
    INode *inode = get(h, ino);
    if(inode) {
        truncate(h, inode, 0, 0);
        h.inodes().free(h, inode->inode, 1);
    }
}

INode *INodes::get(FSHandle &h, inodeno_t ino) {
    size_t inos_per_blk = h.sb().inodes_per_block();
    INode *inos = reinterpret_cast<INode*>(
        h.cache().get_block(h.sb().first_inode_block() + ino / inos_per_blk, false));
    return inos + ino % inos_per_blk;
}

void INodes::stat(FSHandle &, const m3::INode *inode, FileInfo &info) {
    info.devno = inode->devno;
    info.inode = inode->inode;
    info.mode = inode->mode;
    info.links = inode->links;
    info.size = inode->size;
    info.lastaccess = inode->lastaccess;
    info.lastmod = inode->lastmod;
    info.extents = inode->extents;
    info.firstblock = inode->direct[0].start;
}

void INodes::mark_dirty(FSHandle &h, inodeno_t ino) {
    size_t inos_per_blk = h.sb().inodes_per_block();
    h.cache().mark_dirty(h.sb().first_inode_block() + ino / inos_per_blk);
}

void INodes::write_back(FSHandle &h, INode *inode) {
    foreach_block(h, inode, bno)
        h.cache().write_back(bno);
}

size_t INodes::get_extent_mem(FSHandle &h, INode *inode, size_t extent, size_t blocks,
                              int perms, capsel_t sel) {
    Extent *indir = nullptr;
    Extent *ch = get_extent(h, inode, extent, &indir, blocks > 0);
    if(ch == nullptr)
        return 0;

    // extent empty?
    if(ch->length == 0) {
        if(blocks > 0) {
            // fill extent with blocks
            fill_extent(h, inode, ch, blocks);
        }
        if(ch->length == 0)
            return 0;
    }

    size_t left = 0;
    // extend inode size, if we're appending
    if(extent == inode->extents - 1) {
        left = inode->size % h.sb().blocksize;
        if(blocks > 0 && left)
            inode->size += h.sb().blocksize - left;
    }

    // create memory capability for extent
    size_t bytes = ch->length * h.sb().blocksize;
    Errors::Code res = Syscalls::get().derivemem(sel, h.mem().sel(),
                                                 ch->start * h.sb().blocksize, bytes, perms);
    if(res != Errors::NONE)
        return 0;

    // stop at file-end
    if(blocks == 0 && left)
        bytes -= h.sb().blocksize - left;

    return bytes;
}

Extent *INodes::get_extent(FSHandle &h, INode *inode, size_t i, Extent **indir, bool create) {
    if(i < INODE_DIR_COUNT)
        return inode->direct + i;
    i -= INODE_DIR_COUNT;

    // indirect extents
    if(i < h.sb().extents_per_block()) {
        // create indirect block if not done yet
        if(!*indir) {
            bool created = false;
            if(inode->indirect == 0) {
                if(!create)
                    return nullptr;
                inode->indirect = h.blocks().alloc(h);
                created = true;
            }
            // init with zeros
            *indir = reinterpret_cast<Extent*>(h.cache().get_block(inode->indirect, false));
            if(created)
                memset(*indir, 0, h.sb().blocksize);
        }
        // we're going to change it, if its empty and the caller wants to create blocks
        if(create && (*indir)[i].length == 0)
            h.cache().mark_dirty(inode->indirect);
        return &(*indir)[i];
    }

    // double indirect extents
    i -= h.sb().extents_per_block();
    if(i < h.sb().extents_per_block() * h.sb().extents_per_block()) {
        bool created = false;
        // create double indirect block, if not done yet
        if(inode->dindirect == 0) {
            if(!create)
                return nullptr;
            inode->dindirect = h.blocks().alloc(h);
            created = true;
        }
        // init with zeros
        Extent *dindir = reinterpret_cast<Extent*>(h.cache().get_block(inode->dindirect, false));
        if(created)
            memset(dindir, 0, h.sb().blocksize);

        // create indirect block, if necessary
        created = false;
        Extent *ptr = dindir + i / h.sb().extents_per_block();
        if(ptr->length == 0) {
            h.cache().mark_dirty(inode->dindirect);
            ptr->start = h.blocks().alloc(h);
            ptr->length = 1;
            created = true;
        }
        // init with zeros
        dindir = reinterpret_cast<Extent*>(h.cache().get_block(ptr->start, false));
        if(created)
            memset(dindir, 0, h.sb().blocksize);

        // get extent
        Extent *ch = dindir + i % h.sb().extents_per_block();
        if(create && ch->length == 0)
            h.cache().mark_dirty(ptr->start);
        return ch;
    }
    return nullptr;
}

Extent *INodes::change_extent(FSHandle &h, INode *inode, size_t i, Extent **indir, bool remove) {
    if(i < INODE_DIR_COUNT)
        return inode->direct + i;

    i -= INODE_DIR_COUNT;
    if(i < h.sb().extents_per_block()) {
        assert(inode->indirect != 0);
        if(!*indir)
            *indir = reinterpret_cast<Extent*>(h.cache().get_block(inode->indirect, false));

        h.cache().mark_dirty(inode->indirect);

        // we assume that we only delete extents at the end; thus, if its the first, we can remove
        // the indirect block as well.
        if(remove && i == 0) {
            h.blocks().free(h, inode->indirect, 1);
            inode->indirect = 0;
        }
        return &(*indir)[i];
    }

    i -= h.sb().extents_per_block();
    if(i < h.sb().extents_per_block() * h.sb().extents_per_block()) {
        assert(inode->dindirect != 0);
        Extent *dindir = reinterpret_cast<Extent*>(h.cache().get_block(inode->dindirect, false));
        Extent *ptr = dindir + i / h.sb().extents_per_block();
        dindir = reinterpret_cast<Extent*>(h.cache().get_block(ptr->start, false));

        Extent *ch = dindir + i % h.sb().extents_per_block();
        h.cache().mark_dirty(ptr->start);

        // same here; if its the first, remove the indirect-block
        if(remove) {
            if(ch == dindir) {
                h.blocks().free(h, ptr->start, 1);
                ptr->length = 0;
                ptr->start = 0;
                h.cache().mark_dirty(inode->dindirect);
            }

            // and for the double-indirect, too
            if(i == 0) {
                h.blocks().free(h, inode->dindirect, 1);
                inode->dindirect = 0;
            }
        }
        return ch;
    }
    return nullptr;
}

void INodes::fill_extent(FSHandle &h, INode *inode, Extent *ch, uint32_t blocks) {
    size_t count = blocks;
    ch->start = h.blocks().alloc(h, &count);
    if(count == 0) {
        Errors::last = Errors::NO_SPACE;
        ch->length = 0;
        return;
    }
    ch->length = count;
    if(h.clear_blocks()) {
        uint32_t blocksize = h.sb().blocksize;
        Time::start(0xaaaa);
        for(uint32_t i = 0; i < count; ++i)
            h.mem().write(zeros, blocksize, (ch->start + i) * blocksize);
        Time::stop(0xaaaa);
    }
    inode->extents++;
    inode->size = (inode->size + h.sb().blocksize - 1) & ~(h.sb().blocksize - 1);
    inode->size += count * h.sb().blocksize;
    mark_dirty(h, inode->inode);
}

size_t INodes::seek(FSHandle &h, INode *inode, size_t &off, int whence,
                    size_t &extent, size_t &extoff) {
    assert(whence != M3FS_SEEK_CUR);
    Extent *indir = nullptr;

    // seeking to the end is easy
    if(whence == M3FS_SEEK_END) {
        // TODO support off != 0
        assert(off == 0);
        extent = inode->extents;
        extoff = inode->size % h.sb().blocksize;
        if(extoff)
            extent--;
        off = 0;
        return inode->size;
    }

    if(off > inode->size)
        off = inode->size;

    // now search until we've found the extent covering the desired file position
    size_t pos = 0;
    for(size_t i = 0; i < inode->extents; ++i) {
        Extent *ch = get_extent(h, inode, i, &indir, false);
        if(!ch)
            break;

        if(off < ch->length * h.sb().blocksize) {
            extent = i;
            extoff = off;
            return pos;
        }
        pos += ch->length * h.sb().blocksize;
        off -= ch->length * h.sb().blocksize;
    }

    extent = inode->extents;
    extoff = off;
    return pos;
}

void INodes::truncate(FSHandle &h, INode *inode, size_t extent, size_t extoff) {
    Extent *indir = nullptr;
    if(inode->extents > 0) {
        // erase everything up to <extent>
        for(size_t i = inode->extents - 1; i > extent; --i) {
            Extent *ch = change_extent(h, inode, i, &indir, true);
            assert(ch && ch->length > 0);
            h.blocks().free(h, ch->start, ch->length);
            inode->extents--;
            inode->size -= ch->length * h.sb().blocksize;
            ch->start = 0;
            ch->length = 0;
        }

        // get <extent> and determine length
        Extent *ch = change_extent(h, inode, extent, &indir, extoff == 0);
        if(ch && ch->length > 0) {
            size_t curlen = ch->length * h.sb().blocksize;
            size_t mod;
            if((mod = (inode->size % h.sb().blocksize)) != 0)
                curlen -= h.sb().blocksize - mod;

            // do we need to reduce the size of <extent>?
            if(extoff < curlen) {
                size_t diff = curlen - extoff;
                size_t bdiff = extoff == 0 ? Math::round_up<size_t>(diff, h.sb().blocksize) : diff;
                size_t blocks = bdiff / h.sb().blocksize;
                if(blocks > 0)
                    h.blocks().free(h, ch->start + ch->length - blocks, blocks);
                inode->size -= diff;
                ch->length -= blocks;
                if(ch->length == 0) {
                    ch->start = 0;
                    inode->extents--;
                }
            }
        }
        mark_dirty(h, inode->inode);
    }
}
