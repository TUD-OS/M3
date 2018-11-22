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

#include <libgen.h>

#include "Dirs.h"
#include "INodes.h"
#include "Links.h"
#include <base/util/Time.h>
using namespace m3;

static constexpr size_t BUF_SIZE    = 64;

DirEntry *Dirs::find_entry(FSHandle &h, INode *inode, const char *name, size_t namelen, UsedBlocks *used_blocks) {
    foreach_block(h, inode, bno, used_blocks) {
        used_blocks->set(bno);
        foreach_direntry(h, bno, e) {
            if(e->namelen == namelen && strncmp(e->name, name, namelen) == 0) {
                return e;
            }
        }
        used_blocks->quit_last_n(2);
    }
    return nullptr;
}

inodeno_t Dirs::search(FSHandle &h, const char *path, bool create, UsedBlocks *used_blocks) {
    Time::start(0xf000);
    while(*path == '/')
        path++;
    // root inode requested?
    if(*path == '\0') {
        Time::stop(0xf000);
        return 0;
    }

    INode *inode;
    const char *end;
    size_t namelen;
    inodeno_t ino = 0;
    Time::start(0xf001);
    while(1) {
        Time::start(0xf002);
        inode = INodes::get(h, ino, used_blocks);
        Time::stop(0xf002);
        // find path component end
        end = path;
        while(*end && *end != '/')
            end++;

        namelen = static_cast<size_t>(end - path);
        Time::start(0xf003);
        DirEntry *e = find_entry(h, inode, path, namelen, used_blocks);
        Time::stop(0xf003);
        // in any case, skip trailing slashes (see if(create) ...)
        while(*end == '/')
            end++;
        // stop if the file doesn't exist
        if(!e)
            break;
        // if the path is empty, we're done
        if(!*end){
            Time::stop(0xf001);
            Time::stop(0xf000);
            return e->nodeno;
        }

        // to next layer
        ino = e->nodeno;
        path = end;

        used_blocks->quit_last_n(3);
    }
    Time::stop(0xf001);
    Time::start(0xf004);

    if(create) {
        // if there are more path components, we can't create the file
        if(*end) {
            Errors::last = Errors::NO_SUCH_FILE;
            Time::stop(0xf004);
            Time::stop(0xf000);
            return INVALID_INO;
        }

        // create inode and put a link into the directory
        INode *ninode = INodes::create(h, M3FS_IFREG | 0644, used_blocks);
        if(!ninode) {
            Time::stop(0xf004);
            Time::stop(0xf000);
            return INVALID_INO;
        }
        Errors::Code res = Links::create(h, inode, path, namelen, ninode, used_blocks);
        if(res != Errors::NONE) {
            h.files().delete_file(ninode->inode);
            Time::stop(0xf004);
            Time::stop(0xf000);
            return INVALID_INO;
        }
        Time::stop(0xf004);
        Time::stop(0xf000);
        return ninode->inode;
    }

    Errors::last = Errors::NO_SUCH_FILE;
    Time::stop(0xf004);
    Time::stop(0xf000);
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

Errors::Code Dirs::create(FSHandle &h, const char *path, mode_t mode, UsedBlocks *used_blocks) {
    char buf1[BUF_SIZE], buf2[BUF_SIZE], *base, *dir;
    split_path(path, buf1, buf2, &base, &dir);
    size_t baselen = strlen(base);

    // first, get parent directory
    inodeno_t parino = search(h, dir, false, used_blocks);
    if(parino == INVALID_INO)
        return Errors::NO_SUCH_FILE;
    // ensure that the entry doesn't exist
    if(search(h, path, false, used_blocks) != INVALID_INO)
        return Errors::EXISTS;

    INode *parinode = INodes::get(h, parino, used_blocks);
    INode *dirinode = INodes::create(h, M3FS_IFDIR | (mode & 0x777), used_blocks);
    if(dirinode == nullptr)
        return Errors::NO_SPACE;

    // create directory itself
    Errors::Code res = Links::create(h, parinode, base, baselen, dirinode, used_blocks);
    if(res != Errors::NONE)
        goto errINode;

    // create "." and ".."
    res = Links::create(h, dirinode, ".", 1, dirinode, used_blocks);
    if(res != Errors::NONE)
        goto errLink1;
    res = Links::create(h, dirinode, "..", 2, parinode, used_blocks);
    if(res != Errors::NONE)
        goto errLink2;
    return Errors::NONE;

errLink2:
    Links::remove(h, dirinode, ".", 1, true, used_blocks);
errLink1:
    Links::remove(h, parinode, base, baselen, true, used_blocks);
errINode:
    h.files().delete_file(dirinode->inode);
    return res;
}

Errors::Code Dirs::remove(FSHandle &h, const char *path, UsedBlocks *used_blocks) {
    inodeno_t ino = search(h, path, false, used_blocks);
    if(ino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    // it has to be a directory
    INode *inode = INodes::get(h, ino, used_blocks);
    if(!M3FS_ISDIR(inode->mode))
        return Errors::IS_NO_DIR;

    // check whether it's empty
    foreach_block(h, inode, bno, used_blocks) {
        used_blocks->set(bno);
        foreach_direntry(h, bno, e) {
            if(!(e->namelen == 1 && strncmp(e->name, ".", 1) == 0) &&
                !(e->namelen == 2 && strncmp(e->name, "..", 2) == 0)){
                    return Errors::DIR_NOT_EMPTY;
                }
        }
        used_blocks->quit_last_n(2);
    }

    // hardlinks to directories are not possible, thus we always have 2
    assert(inode->links == 2);
    // ensure that the inode is removed
    inode->links--;
    return unlink(h, path, true, used_blocks);
}

Errors::Code Dirs::link(FSHandle &h, const char *oldpath, const char *newpath, UsedBlocks *used_blocks) {
    inodeno_t oldino = search(h, oldpath, false, used_blocks);
    if(oldino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    // is can't be a directory
    INode *oldinode = INodes::get(h, oldino, used_blocks);
    if(M3FS_ISDIR(oldinode->mode))
        return Errors::IS_DIR;

    char buf1[BUF_SIZE], buf2[BUF_SIZE], *base, *dir;
    split_path(newpath, buf1, buf2, &base, &dir);

    inodeno_t dirino = search(h, dir, false, used_blocks);
    if(dirino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    return Links::create(h, INodes::get(h, dirino, used_blocks), base, strlen(base), oldinode, used_blocks);
}

Errors::Code Dirs::unlink(FSHandle &h, const char *path, bool isdir, UsedBlocks *used_blocks) {
    char buf1[BUF_SIZE], buf2[BUF_SIZE], *base, *dir;
    split_path(path, buf1, buf2, &base, &dir);

    inodeno_t parino = search(h, dir, false, used_blocks);
    if(parino == INVALID_INO)
        return Errors::NO_SUCH_FILE;

    return Links::remove(h, INodes::get(h, parino, used_blocks), base, strlen(base), isdir, used_blocks);
}
