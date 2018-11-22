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

#include "Dirs.h"
#include "INodes.h"
#include "Links.h"

using namespace m3;

Errors::Code Links::create(FSHandle &h, INode *dir, const char *name, size_t namelen, INode *inode,
                           UsedBlocks *used_blocks) {
    size_t rem;
    DirEntry *e;

    foreach_block(h, dir, bno, used_blocks) {
        used_blocks->set(bno);
        foreach_direntry(h, bno, de) {
            rem = de->next - (sizeof(DirEntry) + de->namelen);
            if(rem >= sizeof(DirEntry) + namelen) {
                // change previous entry
                de->next = de->namelen + sizeof(DirEntry);
                // get pointer to new one
                e = reinterpret_cast<DirEntry*>(reinterpret_cast<uintptr_t>(de) + de->next);
                h.metabuffer().mark_dirty(bno);
                goto found;
            }
        }
        used_blocks->quit_last_n(2);
    }

    // no suitable space found; extend directory
    {
        Extent *indir = nullptr;
        Extent *ext = INodes::get_extent(h, dir, dir->extents, &indir, true, used_blocks);
        if(!ext)
            return Errors::NO_SPACE;

        // insert one block in extent
        INodes::fill_extent(h, dir, ext, 1, 1);
        if(ext->length == 0)
            return Errors::NO_SPACE;

        // put entry at the beginning of the block
        e = reinterpret_cast<DirEntry*>(h.metabuffer().get_block(ext->start));
        used_blocks->set(ext->start);
        h.metabuffer().mark_dirty(ext->start);
        rem = h.sb().blocksize;
    }

found:
    // write entry
    e->namelen = namelen;
    e->nodeno = inode->inode;
    e->next = rem;
    strncpy(e->name, name, namelen);

    inode->links++;
    INodes::mark_dirty(h, inode->inode);
    return Errors::NONE;
}

Errors::Code Links::remove(FSHandle &h, INode *dir, const char *name, size_t namelen, bool isdir,
                           UsedBlocks *used_blocks) {
    foreach_block(h, dir, bno, used_blocks) {
        used_blocks->set(bno);
        DirEntry *prev = nullptr;
        foreach_direntry(h, bno, e) {
            if(e->namelen == namelen && strncmp(e->name, name, namelen) == 0) {
                // if we're not removing a dir, we're coming from unlink(). in this case, directories
                // are not allowed
                INode *inode = INodes::get(h, e->nodeno, used_blocks);
                if(!isdir && M3FS_ISDIR(inode->mode)) {
                    return Errors::IS_DIR;
                }

                // remove entry by skipping over it
                if(prev)
                    prev->next += e->next;
                // copy the next entry back, if there is any
                else {
                    DirEntry *next = reinterpret_cast<DirEntry*>(reinterpret_cast<char*>(e) + e->next);
                    if(next < __eend) {
                        size_t dist = e->next;
                        memcpy(e, next, sizeof(DirEntry) + next->namelen);
                        e->next = dist + next->next;
                    }
                }
                h.metabuffer().mark_dirty(bno);

                // reduce links and free, if necessary
                if(--inode->links == 0)
                    h.files().delete_file(inode->inode);
                return Errors::NONE;
            }

            prev = e;
        }
        used_blocks->quit_last_n(2);
    }
    return Errors::NO_SUCH_FILE;
}
