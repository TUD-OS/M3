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

#include <libgen.h>

#include "Dirs.h"
#include "INodes.h"
#include "Links.h"

using namespace m3;

static constexpr size_t BUF_SIZE    = 64;

DirEntry *Dirs::find_entry(FSHandle &h, INode *inode, const char *name, size_t namelen) {
    foreach_block(h, inode, bno) {
        foreach_direntry(h, bno, e) {
            if(e->namelen == namelen && strncmp(e->name, name, namelen) == 0)
                return e;
        }
    }
    return NULL;
}

inodeno_t Dirs::search(FSHandle &h, const char *path, bool create) {
    while(*path == '/')
        path++;
    // root inode requested?
    if(*path == '\0')
        return 0;

    INode *inode;
    const char *end;
    size_t namelen;
    inodeno_t ino = 0;
    while(1) {
        inode = INodes::get(h, ino);
        // find path component end
        end = path;
        while(*end && *end != '/')
            end++;

        namelen = end - path;
        DirEntry *e = find_entry(h, inode, path, namelen);
        // in any case, skip trailing slashes (see if(create) ...)
        while(*end == '/')
            end++;
        // stop if the file doesn't exist
        if(!e)
            break;
        // if the path is empty, we're done
        if(!*end)
            return e->nodeno;

        // to next layer
        ino = e->nodeno;
        path = end;
    }

    if(create) {
        // if there are more path components, we can't create the file
        if(*end) {
            Errors::last = Errors::NO_SUCH_FILE;
            return INVALID_INO;
        }

        // create inode and put a link into the directory
        INode *ninode = INodes::create(h, S_IFREG | 0644);
        if(!ninode)
            return INVALID_INO;
        Errors::Code res = Links::create(h, inode, path, namelen, ninode);
        if(res != Errors::NO_ERROR) {
            INodes::free(h, ninode);
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

Errors::Code Dirs::create(FSHandle &h, const char *path, mode_t mode) {
    char buf1[BUF_SIZE], buf2[BUF_SIZE], *base, *dir;
    split_path(path, buf1, buf2, &base, &dir);
    size_t baselen = strlen(base);

    // first, get parent directory
    inodeno_t parino = search(h, dir, false);
    if(parino == INVALID_INO)
        return Errors::NO_SUCH_FILE;
    // ensure that the entry doesn't exist
    if(search(h, path, false) != INVALID_INO)
        return Errors::EXISTS;

    INode *parinode = INodes::get(h, parino);
    INode *dirinode = INodes::create(h, S_IFDIR | (mode & 0x777));
    if(dirinode == NULL)
        return Errors::NO_SPACE;

    // create directory itself
    Errors::Code res = Links::create(h, parinode, base, baselen, dirinode);
    if(res != Errors::NO_ERROR)
        goto errINode;

    // create "." and ".."
    res = Links::create(h, dirinode, ".", 1, dirinode);
    if(res != Errors::NO_ERROR)
        goto errLink1;
    res = Links::create(h, dirinode, "..", 2, parinode);
    if(res != Errors::NO_ERROR)
        goto errLink2;
    return Errors::NO_ERROR;

errLink2:
    Links::remove(h, dirinode, ".", 1, true);
errLink1:
    Links::remove(h, parinode, base, baselen, true);
errINode:
    INodes::free(h, parinode);
    return res;
}

Errors::Code Dirs::remove(FSHandle &h, const char *path) {
    inodeno_t ino = search(h, path, false);
    if(ino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    // it has to be a directory
    INode *inode = INodes::get(h, ino);
    if(!S_ISDIR(inode->mode))
        return Errors::IS_NO_DIR;

    // check whether it's empty
    foreach_block(h, inode, bno) {
        foreach_direntry(h, bno, e) {
            if(!(e->namelen == 1 && strncmp(e->name, ".", 1) == 0) &&
                !(e->namelen == 2 && strncmp(e->name, "..", 2) == 0))
                return Errors::DIR_NOT_EMPTY;
        }
    }

    // hardlinks to directories are not possible, thus we always have 2
    assert(inode->links == 2);
    // ensure that the inode is removed
    inode->links--;
    return unlink(h, path, true);
}

Errors::Code Dirs::link(FSHandle &h, const char *oldpath, const char *newpath) {
    inodeno_t oldino = search(h, oldpath, false);
    if(oldino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    // is can't be a directory
    INode *oldinode = INodes::get(h, oldino);
    if(S_ISDIR(oldinode->mode))
        return Errors::IS_DIR;

    char buf1[BUF_SIZE], buf2[BUF_SIZE], *base, *dir;
    split_path(newpath, buf1, buf2, &base, &dir);

    inodeno_t dirino = search(h, dir, false);
    if(dirino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    return Links::create(h, INodes::get(h, dirino), base, strlen(base), oldinode);
}

Errors::Code Dirs::unlink(FSHandle &h, const char *path, bool isdir) {
    char buf1[BUF_SIZE], buf2[BUF_SIZE], *base, *dir;
    split_path(path, buf1, buf2, &base, &dir);

    inodeno_t parino = search(h, dir, false);
    if(parino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    return Links::remove(h, INodes::get(h, parino), base, strlen(base), isdir);
}
