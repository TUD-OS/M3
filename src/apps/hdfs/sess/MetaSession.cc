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

#include "MetaSession.h"

#include <m3/session/M3FS.h>

#include "../data/Dirs.h"
#include "../data/INodes.h"

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

void M3FSMetaSession::open_private_file(m3::GateIStream &is) {
    int flags;
    uint ep;
    char buffer[64];
    String path(buffer, sizeof(buffer));
    is >> path >> flags >> ep;
    if(ep >= _ep_count) {
        reply_error(is, Errors::INV_ARGS);
        return;
    }

    size_t id;
    Errors::Code res = do_open(ObjCap::INVALID, path.c_str(), flags, &id);
    if(res != Errors::NONE) {
        reply_error(is, res);
        return;
    }

    _files[id]->set_ep(_ep_start + ep);
    reply_vmsg(is, res, id);
}

void M3FSMetaSession::close_private_file(m3::GateIStream &is) {
    size_t id;
    is >> id;
    if(_files[id] != nullptr) {
        delete _files[id];
        _files[id] = nullptr;
    }
    reply_error(is, Errors::NONE);
}

Errors::Code M3FSMetaSession::open_file(capsel_t srv, KIF::Service::ExchangeData &data) {
    if(data.args.count != 1)
        return Errors::INV_ARGS;

    int flags = data.args.svals[0];
    data.args.str[sizeof(data.args.str) - 1] = '\0';
    const char *path = data.args.str;

    size_t id;
    Errors::Code res = do_open(srv, path, flags, &id);
    if(res != Errors::NONE)
        return res;

    data.args.count = 0;
    data.caps       = _files[id]->caps().value();
    return Errors::NONE;
}

static const char *decode_flags(int flags) {
    static char buf[9];
    buf[0] = (flags & FILE_R)       ? 'r' : '-';
    buf[1] = (flags & FILE_W)       ? 'w' : '-';
    buf[2] = (flags & FILE_X)       ? 'x' : '-';
    buf[3] = (flags & FILE_TRUNC)   ? 't' : '-';
    buf[4] = (flags & FILE_APPEND)  ? 'a' : '-';
    buf[5] = (flags & FILE_CREATE)  ? 'c' : '-';
    buf[6] = (flags & FILE_NODATA)  ? 'd' : '-';
    buf[7] = (flags & FILE_NOSESS)  ? 's' : '-';
    buf[8] = '\0';
    return buf;
}

Errors::Code M3FSMetaSession::do_open(capsel_t srv, const char *path, int flags, size_t *id) {
    PRINT(this, "fs::open(path=" << path << ", flags=" << decode_flags(flags) << ")");

    Request r(hdl());

    inodeno_t ino = Dirs::search(r, path, flags & FILE_CREATE);
    if(ino == INVALID_INO) {
        PRINT(this, "open failed: " << Errors::to_string(Errors::last));
        return Errors::last;
    }

    INode *inode = INodes::get(r, ino);
    if(((flags & FILE_W) && (~inode->mode & M3FS_IWUSR)) ||
       ((flags & FILE_R) && (~inode->mode & M3FS_IRUSR))) {
        PRINT(this, "open failed: " << Errors::to_string(Errors::NO_PERM));
        return Errors::NO_PERM;
    }

    // only determine the current size, if we're writing and the file isn't empty
    if(flags & FILE_TRUNC) {
        INodes::truncate(r, inode, 0, 0);
        // TODO revoke access, if necessary
    }

    // for directories: ensure that we don't have a changed version in the cache
    if(M3FS_ISDIR(inode->mode))
        INodes::sync_metadata(r, inode);
    ssize_t res = alloc_file(srv, path, flags, inode->inode);
    if(res < 0)
        return static_cast<Errors::Code>(-res);

    *id = static_cast<size_t>(res);
    PRINT(this, "-> inode=" << inode->inode << ", id=" << *id);

    return Errors::NONE;
}

void M3FSMetaSession::next_in(GateIStream &is) {
    size_t id;
    is >> id;
    if(_files[id] != nullptr)
        _files[id]->next_in(is);
    else
        reply_error(is, Errors::INV_ARGS);
}

void M3FSMetaSession::next_out(GateIStream &is) {
    size_t id;
    is >> id;
    if(_files[id] != nullptr)
        _files[id]->next_out(is);
    else
        reply_error(is, Errors::INV_ARGS);
}

void M3FSMetaSession::commit(GateIStream &is) {
    size_t id;
    is >> id;
    if(_files[id] != nullptr)
        _files[id]->commit(is);
    else
        reply_error(is, Errors::INV_ARGS);
}

void M3FSMetaSession::seek(GateIStream &is) {
    size_t id;
    is >> id;
    if(_files[id] != nullptr)
        _files[id]->seek(is);
    else
        reply_error(is, Errors::INV_ARGS);
}

void M3FSMetaSession::fstat(GateIStream &is) {
    size_t id;
    is >> id;
    if(_files[id] != nullptr)
        _files[id]->fstat(is);
    else
        reply_error(is, Errors::INV_ARGS);
}

void M3FSMetaSession::stat(GateIStream &is) {
    EVENT_TRACER_FS_stat();
    String path;
    is >> path;

    Request r(hdl());

    PRINT(this, "fs::stat(path=" << path << ")");

    m3::inodeno_t ino = Dirs::search(r, path.c_str(), false);
    if(ino == INVALID_INO) {
        PRINT(this, "stat failed: " << Errors::to_string(Errors::last));
        reply_error(is, Errors::last);
        return;
    }

    m3::INode *inode = INodes::get(r, ino);
    assert(inode != nullptr);

    m3::FileInfo info;
    INodes::stat(r, inode, info);
    reply_vmsg(is, Errors::NONE, info);
}

void M3FSMetaSession::mkdir(GateIStream &is) {
    EVENT_TRACER_FS_mkdir();
    String path;
    mode_t mode;
    is >> path >> mode;

    Request r(hdl());

    PRINT(this, "fs::mkdir(path=" << path << ", mode=" << fmt(mode, "o") << ")");

    Errors::Code res = Dirs::create(r, path.c_str(), mode);
    if(res != Errors::NONE)
        PRINT(this, "mkdir failed: " << Errors::to_string(res));
    reply_error(is, res);
}

void M3FSMetaSession::rmdir(GateIStream &is) {
    EVENT_TRACER_FS_rmdir();
    String path;
    is >> path;

    Request r(hdl());

    PRINT(this, "fs::rmdir(path=" << path << ")");

    Errors::Code res = Dirs::remove(r, path.c_str());
    if(res != Errors::NONE)
        PRINT(this, "rmdir failed: " << Errors::to_string(res));
    reply_error(is, res);
}

void M3FSMetaSession::link(GateIStream &is) {
    EVENT_TRACER_FS_link();
    String oldpath, newpath;
    is >> oldpath >> newpath;

    Request r(hdl());

    PRINT(this, "fs::link(oldpath=" << oldpath << ", newpath=" << newpath << ")");

    Errors::Code res = Dirs::link(r, oldpath.c_str(), newpath.c_str());
    if(res != Errors::NONE)
        PRINT(this, "link failed: " << Errors::to_string(res));
    reply_error(is, res);
}

void M3FSMetaSession::unlink(GateIStream &is) {
    EVENT_TRACER_FS_unlink();
    String path;
    is >> path;

    Request r(hdl());

    PRINT(this, "fs::unlink(path=" << path << ")");

    Errors::Code res = Dirs::unlink(r, path.c_str(), false);
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

ssize_t M3FSMetaSession::alloc_file(capsel_t srv, const char *path, int flags, inodeno_t ino) {
    assert(flags != 0);
    for(size_t i = 0; i < MAX_FILES; ++i) {
        if(_files[i] == NULL) {
            _files[i] = new M3FSFileSession(hdl(), srv, this, path, flags, ino);
            return static_cast<ssize_t>(i);
        }
    }
    return -Errors::NO_SPACE;
}
