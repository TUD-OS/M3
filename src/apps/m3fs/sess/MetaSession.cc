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

#include <m3/session/M3FS.h>

#include "../data/Dirs.h"
#include "../data/INodes.h"
#include "MetaSession.h"

using namespace m3;

Errors::Code M3FSMetaSession::get_sgate(KIF::Service::ExchangeData &data) {
    if(data.caps != 1)
        return Errors::INV_ARGS;

    label_t label = reinterpret_cast<label_t>(this);
    MetaSGate *sgate = new MetaSGate(SendGate::create(&_rgate, label, MSG_SIZE));
    _sgates.append(sgate);

    data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sgate->sgate.sel()).value();
    return Errors::NONE;
}

Errors::Code M3FSMetaSession::open_file(capsel_t srv, KIF::Service::ExchangeData &data) {
    if(data.args.count != 1)
        return Errors::INV_ARGS;

    int flags = data.args.svals[0];
    data.args.str[sizeof(data.args.str) - 1] = '\0';
    const char *path = data.args.str;

    PRINT(this, "fs::open(path=" << path << ", flags=" << fmt(flags, "#x") << ")");

    inodeno_t ino = Dirs::search(handle(), path, flags & FILE_CREATE);
    if(ino == INVALID_INO) {
        PRINT(this, "open failed: " << Errors::to_string(Errors::last));
        return Errors::last;
    }

    INode *inode = INodes::get(handle(), ino);
    if(((flags & FILE_W) && (~inode->mode & M3FS_IWUSR)) ||
        ((flags & FILE_R) && (~inode->mode & M3FS_IRUSR))) {
        PRINT(this, "open failed: " << Errors::to_string(Errors::NO_PERM));
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

    PRINT(this, "-> inode=" << inode->inode);
    return Errors::NONE;
}

void M3FSMetaSession::stat(GateIStream &is) {
    EVENT_TRACER_FS_stat();
    String path;
    is >> path;
    PRINT(this, "fs::stat(path=" << path << ")");

    m3::inodeno_t ino = Dirs::search(_handle, path.c_str(), false);
    if(ino == INVALID_INO) {
        PRINT(this, "stat failed: " << Errors::to_string(Errors::last));
        reply_error(is, Errors::last);
        return;
    }

    m3::INode *inode = INodes::get(_handle, ino);
    assert(inode != nullptr);

    m3::FileInfo info;
    INodes::stat(_handle, inode, info);
    reply_vmsg(is, Errors::NONE, info);
}

void M3FSMetaSession::mkdir(GateIStream &is) {
    EVENT_TRACER_FS_mkdir();
    String path;
    mode_t mode;
    is >> path >> mode;
    PRINT(this, "fs::mkdir(path=" << path << ", mode=" << fmt(mode, "o") << ")");

    Errors::Code res = Dirs::create(_handle, path.c_str(), mode);
    if(res != Errors::NONE)
        PRINT(this, "mkdir failed: " << Errors::to_string(res));
    reply_error(is, res);
}

void M3FSMetaSession::rmdir(GateIStream &is) {
    EVENT_TRACER_FS_rmdir();
    String path;
    is >> path;
    PRINT(this, "fs::rmdir(path=" << path << ")");

    Errors::Code res = Dirs::remove(_handle, path.c_str());
    if(res != Errors::NONE)
        PRINT(this, "rmdir failed: " << Errors::to_string(res));
    reply_error(is, res);
}

void M3FSMetaSession::link(GateIStream &is) {
    EVENT_TRACER_FS_link();
    String oldpath, newpath;
    is >> oldpath >> newpath;
    PRINT(this, "fs::link(oldpath=" << oldpath << ", newpath=" << newpath << ")");

    Errors::Code res = Dirs::link(_handle, oldpath.c_str(), newpath.c_str());
    if(res != Errors::NONE)
        PRINT(this, "link failed: " << Errors::to_string(res));
    reply_error(is, res);
}

void M3FSMetaSession::unlink(GateIStream &is) {
    EVENT_TRACER_FS_unlink();
    String path;
    is >> path;
    PRINT(this, "fs::unlink(path=" << path << ")");

    Errors::Code res = Dirs::unlink(_handle, path.c_str(), false);
    if(res != Errors::NONE)
        PRINT(this, "unlink failed: " << Errors::to_string(res));
    reply_error(is, res);
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
