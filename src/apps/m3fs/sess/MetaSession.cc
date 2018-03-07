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

#include <base/log/Services.h>

#include "MetaSession.h"
#include "../Dirs.h"
#include "../INodes.h"

using namespace m3;

void M3FSMetaSession::close() {
    for(size_t i = 0; i < MAX_FILES; ++i)
        delete _files[i];
}

Errors::Code M3FSMetaSession::open_file(capsel_t srv, KIF::Service::ExchangeData &data) {
    if(data.args.count != 1)
        return Errors::INV_ARGS;

    int flags = data.args.svals[0];
    data.args.str[sizeof(data.args.str) - 1] = '\0';
    const char *path = data.args.str;

    SLOG(FS, fmt((word_t)this, "#x") << ": fs::open(path=" << path
        << ", flags=" << fmt(flags, "#x") << ")");

    inodeno_t ino = Dirs::search(handle(), path, flags & FILE_CREATE);
    if(ino == INVALID_INO) {
        SLOG(FS, fmt((word_t)this, "#x") << ": open failed: " << Errors::to_string(Errors::last));
        return Errors::last;
    }

    INode *inode = INodes::get(handle(), ino);
    if(((flags & FILE_W) && (~inode->mode & M3FS_IWUSR)) ||
        ((flags & FILE_R) && (~inode->mode & M3FS_IRUSR))) {
        SLOG(FS, fmt((word_t)this, "#x") << ": open failed: " << Errors::to_string(Errors::NO_PERM));
        return Errors::NO_PERM;
    }

    // only determine the current size, if we're writing and the file isn't empty
    if(flags & FILE_TRUNC) {
        INodes::truncate(_handle, inode, 0, 0);
        // TODO revoke access, if necessary
    }

    // for directories: ensure that we don't have a changed version in the cache
    if(M3FS_ISDIR(inode->mode))
        INodes::write_back(_handle, inode);

    ssize_t res = alloc_file(srv, path, flags, *inode);
    if(res < 0)
        return static_cast<Errors::Code>(-res);

    data.args.count = 0;
    data.caps = _files[res]->caps().value();

    SLOG(FS, fmt((word_t)this, "#x") << ": -> inode=" << inode->inode);
    return Errors::NONE;
}

void M3FSMetaSession::remove_file(M3FSFileSession *file) {
    for(size_t i = 0; i < MAX_FILES; ++i) {
        if(_files[i] == file) {
            _files[i] = nullptr;
            break;
        }
    }
}

ssize_t M3FSMetaSession::alloc_file(capsel_t srv, const char *path, int flags, const INode &inode) {
    assert(flags != 0);
    for(size_t i = 0; i < MAX_FILES; ++i) {
        if(_files[i] == NULL) {
            _files[i] = new M3FSFileSession(srv, this, path, flags, inode);
            return static_cast<ssize_t>(i);
        }
    }
    return -Errors::NO_SPACE;
}
