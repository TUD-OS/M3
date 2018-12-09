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

#include "../FSHandle.h"
#include "INodes.h"

using namespace m3;

INode *INodes::create(Request &r, mode_t mode) {
    inodeno_t ino = r.hdl().inodes().alloc(r);
    if(ino == 0) {
        Errors::last = Errors::NO_SPACE;
        return nullptr;
    }
    INode *inode = get(r, ino);
    memset(inode, 0, sizeof(*inode));
    inode->inode = ino;
    inode->devno = 0; /* TODO */
    inode->mode = mode;
    mark_dirty(r, ino);
    return inode;
}

void INodes::free(Request &r, inodeno_t ino) {
    INode *inode = get(r, ino);
    if(inode) {
        truncate(r, inode, 0, 0);
        r.hdl().inodes().free(r, inode->inode, 1);
    }
}

INode *INodes::get(Request &r, inodeno_t ino) {
    size_t inos_per_blk = r.hdl().sb().inodes_per_block();
    blockno_t bno = r.hdl().sb().first_inode_block() + ino / inos_per_blk;
    INode *inos = reinterpret_cast<INode*>(r.hdl().metabuffer().get_block(r, bno));
    INode *inode = inos + ino % inos_per_blk;
    return inode;
}

void INodes::stat(Request &, const m3::INode *inode, FileInfo &info) {
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

void INodes::mark_dirty(Request &r, inodeno_t ino) {
    size_t inos_per_blk = r.hdl().sb().inodes_per_block();
    r.hdl().metabuffer().mark_dirty(r.hdl().sb().first_inode_block() + ino / inos_per_blk);
}

void INodes::sync_metadata(Request &r, INode *inode) {
    size_t org_used = r.used_meta();
    foreach_extent(r, inode, ext) {
        foreach_block(ext, bno) {
            if(r.hdl().metabuffer().dirty(bno))
                r.hdl().backend()->sync_meta(r, bno);
        }
        r.pop_meta(r.used_meta() - org_used);
    }
}

size_t INodes::get_extent_mem(Request &r, INode *inode, size_t extent, size_t extoff, size_t *extlen,
                              int perms, capsel_t sel, bool dirty, size_t accessed) {
    Extent *indir = nullptr;
    Extent *ext = get_extent(r, inode, extent, &indir, false);
    if(ext == nullptr || ext->length == 0)
        return 0;

    // create memory capability for extent
    uint32_t blocksize = r.hdl().sb().blocksize;
    *extlen = ext->length * blocksize;
    size_t bytes = r.hdl().backend()->get_filedata(r, ext, extoff, perms, sel, dirty, true, accessed);
    if(bytes == 0)
        return 0;

    // stop at file-end
    if(extent == inode->extents - 1 && ext->length * blocksize <= extoff + bytes) {
        size_t rem = inode->size % blocksize;
        if(rem > 0) {
            bytes -= blocksize - rem;
            *extlen -= blocksize - rem;
        }
    }
    return bytes;
}

size_t INodes::req_append(Request &r, INode *inode, size_t i, size_t extoff, size_t *extlen,
                          capsel_t sel, int perm, Extent *ext, size_t accessed) {
    bool load = true;
    if(i < inode->extents) {
        Extent *indir = nullptr;
        ext = get_extent(r, inode, i, &indir, false);
        assert(ext != nullptr);
    }
    else {
        fill_extent(r, nullptr, ext, r.hdl().extend(), accessed);
        // this is a new extent we dont have to load it
        if(!r.hdl().clear_blocks())
            load = false;
        if(Errors::occurred())
            return 0;
        extoff = 0;
    }

    *extlen = ext->length * r.hdl().sb().blocksize;
    return r.hdl().backend()->get_filedata(r, ext, extoff, perm, sel, true, load, accessed);
}

Errors::Code INodes::append_extent(Request &r, INode *inode, Extent *next, size_t *prev_ext_len) {
    Extent *indir = nullptr;

    Extent *ext = nullptr;

    *prev_ext_len = 0;
    if(inode->extents > 0) {
        ext = INodes::get_extent(r, inode, inode->extents - 1, &indir, false);
        assert(ext != nullptr);
        if(ext->start + ext->length != next->start)
            ext = nullptr;
        else
            *prev_ext_len = ext->length * r.hdl().sb().blocksize;
    }

    if(ext == nullptr) {
        ext = INodes::get_extent(r, inode, inode->extents, &indir, true);
        if(!ext)
            return Errors::NO_SPACE;
        ext->start = next->start;
        inode->extents++;
    }

    ext->length += next->length;
    return Errors::NONE;
}

Extent *INodes::get_extent(Request &r, INode *inode, size_t i, Extent **indir, bool create) {
    if(i < INODE_DIR_COUNT)
        return inode->direct + i;
    i -= INODE_DIR_COUNT;

    // indirect extents
    if(i < r.hdl().sb().extents_per_block()) {
        // create indirect block if not done yet
        if(!*indir) {
            bool created = false;
            if(inode->indirect == 0) {
                if(!create)
                    return nullptr;
                inode->indirect = r.hdl().blocks().alloc(r);
                created = true;
            }
            // init with zeros
            *indir = reinterpret_cast<Extent*>(r.hdl().metabuffer().get_block(r, inode->indirect));
            if(created)
                memset(*indir, 0, r.hdl().sb().blocksize);
        }
        // we're going to change it, if its empty and the caller wants to create blocks
        if(create && (*indir)[i].length == 0)
            r.hdl().metabuffer().mark_dirty(inode->indirect);
        return &(*indir)[i];
    }

    // double indirect extents
    i -= r.hdl().sb().extents_per_block();
    if(i < r.hdl().sb().extents_per_block() * r.hdl().sb().extents_per_block()) {
        bool created = false;
        // create double indirect block, if not done yet
        if(inode->dindirect == 0) {
            if(!create)
                return nullptr;
            inode->dindirect = r.hdl().blocks().alloc(r);
            created = true;
        }
        // init with zeros
        Extent *dindir = reinterpret_cast<Extent*>(r.hdl().metabuffer().get_block(r, inode->dindirect));
        if(created)
            memset(dindir, 0, r.hdl().sb().blocksize);

        // create indirect block, if necessary
        created = false;
        Extent *ptr = dindir + i / r.hdl().sb().extents_per_block();
        if(ptr->length == 0) {
            r.hdl().metabuffer().mark_dirty(inode->dindirect);
            ptr->start = r.hdl().blocks().alloc(r);
            ptr->length = 1;
            created = true;
        }
        // init with zeros
        dindir = reinterpret_cast<Extent*>(r.hdl().metabuffer().get_block(r, ptr->start));
        if(created)
            memset(dindir, 0, r.hdl().sb().blocksize);

        // get extent
        Extent *ext = dindir + i % r.hdl().sb().extents_per_block();
        if(create && ext->length == 0)
            r.hdl().metabuffer().mark_dirty(ptr->start);
        return ext;
    }
    return nullptr;
}

Extent *INodes::change_extent(Request &r, INode *inode, size_t i, Extent **indir, bool remove) {
    if(i < INODE_DIR_COUNT)
        return inode->direct + i;

    i -= INODE_DIR_COUNT;
    if(i < r.hdl().sb().extents_per_block()) {
        assert(inode->indirect != 0);
        if(!*indir) {
            *indir = reinterpret_cast<Extent*>(r.hdl().metabuffer().get_block(r, inode->indirect));
        }

        r.hdl().metabuffer().mark_dirty(inode->indirect);

        // we assume that we only delete extents at the end; thus, if its the first, we can remove
        // the indirect block as well.
        if(remove && i == 0) {
            r.hdl().blocks().free(r, inode->indirect, 1);
            inode->indirect = 0;
        }
        return &(*indir)[i];
    }

    i -= r.hdl().sb().extents_per_block();
    if(i < r.hdl().sb().extents_per_block() * r.hdl().sb().extents_per_block()) {
        assert(inode->dindirect != 0);
        Extent *dindir = reinterpret_cast<Extent*>(r.hdl().metabuffer().get_block(r, inode->dindirect));
        Extent *ptr = dindir + i / r.hdl().sb().extents_per_block();
        dindir = reinterpret_cast<Extent*>(r.hdl().metabuffer().get_block(r, ptr->start));

        Extent *ext = dindir + i % r.hdl().sb().extents_per_block();
        r.hdl().metabuffer().mark_dirty(ptr->start);

        // same here; if its the first, remove the indirect-block
        if(remove) {
            if(ext == dindir) {
                r.hdl().blocks().free(r, ptr->start, 1);
                ptr->length = 0;
                ptr->start = 0;
                r.hdl().metabuffer().mark_dirty(inode->dindirect);
            }

            // and for the double-indirect, too
            if(i == 0) {
                r.hdl().blocks().free(r, inode->dindirect, 1);
                inode->dindirect = 0;
            }
        }
        return ext;
    }
    return nullptr;
}

void INodes::fill_extent(Request &r, INode *inode, Extent *ext, uint32_t blocks, size_t accessed) {
    size_t count = blocks;
    ext->start = r.hdl().blocks().alloc(r, &count);
    if(count == 0) {
        Errors::last = Errors::NO_SPACE;
        ext->length = 0;
        return;
    }
    ext->length = count;

    uint32_t blocksize = r.hdl().sb().blocksize;
    if(r.hdl().clear_blocks()) {
        Time::start(0xaaaa);
        r.hdl().backend()->clear_extent(r, ext, accessed);
        Time::stop(0xaaaa);
    }

    if(inode) {
        inode->extents++;
        inode->size = (inode->size + blocksize - 1) & ~(blocksize - 1);
        inode->size += count * blocksize;
        mark_dirty(r, inode->inode);
    }
}

size_t INodes::seek(Request &r, INode *inode, size_t &off, int whence, size_t &extent, size_t &extoff) {
    assert(whence != M3FS_SEEK_CUR);
    Extent *indir = nullptr;
    uint32_t blocksize = r.hdl().sb().blocksize;

    // seeking to the end is easy
    if(whence == M3FS_SEEK_END) {
        // TODO support off != 0
        assert(off == 0);
        extent = inode->extents;
        extoff = 0;
        // determine extent offset
        if(extent > 0) {
            Extent *ext = get_extent(r, inode, extent - 1, &indir, false);
            assert(ext != nullptr);
            extoff = ext->length * blocksize;
            // ensure to stay within the file size
            size_t unaligned = inode->size % blocksize;
            if(unaligned)
                extoff -= blocksize - unaligned;
        }
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
        Extent *ext = get_extent(r, inode, i, &indir, false);
        if(!ext)
            break;

        if(off < ext->length * blocksize) {
            extent = i;
            extoff = off;
            return pos;
        }
        pos += ext->length * blocksize;
        off -= ext->length * blocksize;
    }

    extent = inode->extents;
    extoff = off;
    return pos;
}

void INodes::truncate(Request &r, INode *inode, size_t extent, size_t extoff) {
    uint32_t blocksize = r.hdl().sb().blocksize;

    Extent *indir = nullptr;
    if(inode->extents > 0) {
        // erase everything up to <extent>
        for(size_t i = inode->extents - 1; i > extent; --i) {
            Extent *ext = change_extent(r, inode, i, &indir, true);
            assert(ext && ext->length > 0);
            r.hdl().blocks().free(r, ext->start, ext->length);
            inode->extents--;
            inode->size -= ext->length * blocksize;
            ext->start = 0;
            ext->length = 0;
        }

        // get <extent> and determine length
        Extent *ext = change_extent(r, inode, extent, &indir, extoff == 0);
        if(ext && ext->length > 0) {
            size_t curlen = ext->length * blocksize;
            size_t mod;
            if((mod = (inode->size % blocksize)) != 0)
                curlen -= blocksize - mod;

            // do we need to reduce the size of <extent>?
            if(extoff < curlen) {
                size_t diff = curlen - extoff;
                size_t bdiff = extoff == 0 ? Math::round_up<size_t>(diff, blocksize) : diff;
                size_t blocks = bdiff / blocksize;
                if(blocks > 0)
                    r.hdl().blocks().free(r, ext->start + ext->length - blocks, blocks);
                inode->size -= diff;
                ext->length -= blocks;
                if(ext->length == 0) {
                    ext->start = 0;
                    inode->extents--;
                }
            }
        }
        mark_dirty(r, inode->inode);
    }
}
