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

#include <libgen.h>

#include "Dirs.h"
#include "INodes.h"
#include "Links.h"

using namespace m3;

static constexpr size_t BUF_SIZE = 64;

DirEntry *Dirs::find_entry(Request &r, INode *inode, const char *name, size_t namelen) {
    size_t org_used = r.used_meta();
    foreach_extent(r, inode, ext) {
        foreach_block(ext, bno) {
            foreach_direntry(r, bno, e) {
                if(e->namelen == namelen && strncmp(e->name, name, namelen) == 0)
                    return e;
            }
            r.pop_meta();
        }
        r.pop_meta(r.used_meta() - org_used);
    }
    return nullptr;
}

inodeno_t Dirs::search(Request &r, const char *path, bool create) {
    while(*path == '/')
        path++;
    // root inode requested?
    if(*path == '\0')
        return 0;

    INode *inode;
    const char *end;
    size_t namelen;
    inodeno_t ino = 0;
    size_t org_used = r.used_meta();
    while(1) {
        inode = INodes::get(r, ino);
        // find path component end
        end = path;
        while(*end && *end != '/')
            end++;

        namelen = static_cast<size_t>(end - path);
        DirEntry *e = find_entry(r, inode, path, namelen);
        // in any case, skip trailing slashes (see if(create) ...)
        while(*end == '/')
            end++;
        // stop if the file doesn't exist
        if(!e) {
            r.pop_meta();
            break;
        }
        // if the path is empty, we're done
        if(!*end) {
            r.pop_meta(r.used_meta() - org_used);
            return e->nodeno;
        }

        // to next layer
        ino = e->nodeno;
        path = end;

        r.pop_meta(r.used_meta() - org_used);
    }

    if(create) {
        // if there are more path components, we can't create the file
        if(*end) {
            Errors::last = Errors::NO_SUCH_FILE;
            return INVALID_INO;
        }

        // create inode and put a link into the directory
        INode *ninode = INodes::create(r, M3FS_IFREG | 0644);
        if(!ninode) {
            return INVALID_INO;
        }
        Errors::Code res = Links::create(r, inode, path, namelen, ninode);
        if(res != Errors::NONE) {
            r.hdl().files().delete_file(ninode->inode);
            return INVALID_INO;
        }
        return ninode->inode;
    }

    Errors::last = Errors::NO_SUCH_FILE;
    return INVALID_INO;
}

static void split_path(const char *path, char *buf1, char *buf2, char **base, char **dir) {
    strncpy(buf1, path, BUF_SIZE);
    buf1[BUF_SIZE - 1] = '\0';
    strncpy(buf2, path, BUF_SIZE);
    buf2[BUF_SIZE - 1] = '\0';
    *base = basename(buf1);
    *dir = dirname(buf2);
}

Errors::Code Dirs::create(Request &r, const char *path, mode_t mode) {
    char buf1[BUF_SIZE], buf2[BUF_SIZE], *base, *dir;
    split_path(path, buf1, buf2, &base, &dir);
    size_t baselen = strlen(base);

    // first, get parent directory
    inodeno_t parino = search(r, dir, false);
    if(parino == INVALID_INO)
        return Errors::NO_SUCH_FILE;
    // ensure that the entry doesn't exist
    if(search(r, path, false) != INVALID_INO)
        return Errors::EXISTS;

    INode *parinode = INodes::get(r, parino);
    INode *dirinode = INodes::create(r, M3FS_IFDIR | (mode & 0x777));
    if(dirinode == nullptr)
        return Errors::NO_SPACE;

    // create directory itself
    Errors::Code res = Links::create(r, parinode, base, baselen, dirinode);
    if(res != Errors::NONE)
        goto errINode;

    // create "." and ".."
    res = Links::create(r, dirinode, ".", 1, dirinode);
    if(res != Errors::NONE)
        goto errLink1;
    res = Links::create(r, dirinode, "..", 2, parinode);
    if(res != Errors::NONE)
        goto errLink2;
    return Errors::NONE;

errLink2:
    Links::remove(r, dirinode, ".", 1, true);
errLink1:
    Links::remove(r, parinode, base, baselen, true);
errINode:
    r.hdl().files().delete_file(dirinode->inode);
    return res;
}

Errors::Code Dirs::remove(Request &r, const char *path) {
    inodeno_t ino = search(r, path, false);
    if(ino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    // it has to be a directory
    INode *inode = INodes::get(r, ino);
    if(!M3FS_ISDIR(inode->mode))
        return Errors::IS_NO_DIR;

    // check whether it's empty
    size_t org_used = r.used_meta();
    foreach_extent(r, inode, ext) {
        foreach_block(ext, bno) {
            foreach_direntry(r, bno, e) {
                if(!(e->namelen == 1 && strncmp(e->name, ".", 1) == 0) &&
                   !(e->namelen == 2 && strncmp(e->name, "..", 2) == 0)) {
                    r.pop_meta(r.used_meta() - org_used);
                    return Errors::DIR_NOT_EMPTY;
                }
            }
            r.pop_meta();
        }
        r.pop_meta(r.used_meta() - org_used);
    }

    // hardlinks to directories are not possible, thus we always have 2
    assert(inode->links == 2);
    // ensure that the inode is removed
    inode->links--;
    return unlink(r, path, true);
}

Errors::Code Dirs::link(Request &r, const char *oldpath, const char *newpath) {
    inodeno_t oldino = search(r, oldpath, false);
    if(oldino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    // is can't be a directory
    INode *oldinode = INodes::get(r, oldino);
    if(M3FS_ISDIR(oldinode->mode))
        return Errors::IS_DIR;

    char buf1[BUF_SIZE], buf2[BUF_SIZE], *base, *dir;
    split_path(newpath, buf1, buf2, &base, &dir);

    inodeno_t dirino = search(r, dir, false);
    if(dirino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    return Links::create(r, INodes::get(r, dirino), base, strlen(base), oldinode);
}

Errors::Code Dirs::unlink(Request &r, const char *path, bool isdir) {
    char buf1[BUF_SIZE], buf2[BUF_SIZE], *base, *dir;
    split_path(path, buf1, buf2, &base, &dir);

    inodeno_t parino = search(r, dir, false);
    if(parino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    return Links::remove(r, INodes::get(r, parino), base, strlen(base), isdir);
}
