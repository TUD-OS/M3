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
#include <m3/Syscalls.h>

#include "../data/INodes.h"
#include "FileSession.h"
#include "MetaSession.h"

using namespace m3;

M3FSFileSession::M3FSFileSession(capsel_t srv, M3FSMetaSession *meta, const m3::String &filename,
                                 int flags, m3::inodeno_t ino)
    : M3FSSession(),
      m3::SListItem(),
      _extent(),
      _extoff(),
      _lastoff(),
      _extlen(),
      _fileoff(),
      _appending(),
      _append_ext(),
      _last(ObjCap::INVALID),
      _epcap(ObjCap::INVALID),
      _sess(m3::VPE::self().alloc_sels(2)),
      _sgate(m3::SendGate::create(&meta->rgate(), reinterpret_cast<label_t>(this),
                                  MSG_SIZE, nullptr, _sess + 1)),
      _oflags(flags),
      _filename(filename),
      _ino(ino),
      _capscon(),
      _meta(meta) {
    Syscalls::get().createsessat(_sess, srv, reinterpret_cast<word_t>(this));

    _meta->handle().files().add_sess(this);
}

M3FSFileSession::~M3FSFileSession() {
    PRINT(this, "file::close(path=" << _filename << ")");

    if(_append_ext) {
        FSHandle &h = _meta->handle();
        h.blocks().free(h, _append_ext->start, _append_ext->length);
        delete _append_ext;
    }

    _meta->handle().files().rem_sess(this);
    _meta->remove_file(this);

    if(_last != ObjCap::INVALID)
        VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _last, 1));
}

Errors::Code M3FSFileSession::clone(capsel_t srv, KIF::Service::ExchangeData &data) {
    PRINT(this, "file::clone(path=" << _filename << ")");

    auto nfile =  new M3FSFileSession(srv, _meta, _filename, _oflags, _ino);

    data.args.count = 0;
    data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, nfile->_sess, 2).value();

    return Errors::NONE;
}

Errors::Code M3FSFileSession::get_mem(KIF::Service::ExchangeData &data) {
    EVENT_TRACER_FS_getlocs();
    if(data.args.count != 1)
        return Errors::INV_ARGS;

    size_t offset = data.args.vals[0];

    PRINT(this, "file::get_mem(path=" << _filename << ", offset=" << offset << ")");

    INode *inode = INodes::get(_meta->handle(), _ino);
    assert(inode != nullptr);

    // determine extent from byte offset
    size_t firstOff = offset;
    {
        size_t tmp_extent, tmp_extoff;
        INodes::seek(_meta->handle(), inode, firstOff, M3FS_SEEK_SET, tmp_extent, tmp_extoff);
        offset = tmp_extent;
    }

    capsel_t sel = VPE::self().alloc_sel();
    Errors::last = Errors::NONE;
    size_t len = INodes::get_extent_mem(_meta->handle(), inode, offset,
                                        _oflags & MemGate::RWX, sel);
    if(Errors::occurred()) {
        PRINT(this, "getting extent memory failed: " << Errors::to_string(Errors::last));
        return Errors::last;
    }

    data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel, 1).value();
    data.args.count = 2;
    data.args.vals[0] = firstOff;
    data.args.vals[1] = len;

    PRINT(this, "file::get_mem -> " << len);

    _capscon.add(sel);
    return Errors::NONE;
}

void M3FSFileSession::read_write(GateIStream &is, bool write) {
    size_t submit;
    is >> submit;

    PRINT(this, "file::" << (write ? "write" : "read") << "(submit=" << submit << "); "
        << "file[path=" << _filename << ", fileoff=" << _fileoff << ", ext=" << _extent
        << ", extoff=" << _extoff << "]");

    if((write && !(_oflags & FILE_W)) || (!write && !(_oflags & FILE_R))) {
        reply_error(is, Errors::NO_PERM);
        return;
    }

    FSHandle &h = _meta->handle();
    INode *inode = INodes::get(h, _ino);
    assert(inode != nullptr);

    if(submit > 0) {
        if(_extent == 0) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        Errors::Code res = write ? commit(inode, submit) : Errors::NONE;
        if(res == Errors::NONE && _lastoff + submit < _extlen) {
            _extent--;
            _extoff = _lastoff + submit;
        }
        reply_vmsg(is, res, inode->size);
        return;
    }
    else if(write && _appending) {
        Errors::Code res = commit(inode, _extlen - _lastoff);
        if(res != Errors::NONE) {
            reply_error(is, res);
            return;
        }
    }

    Errors::last = Errors::NONE;
    capsel_t sel = VPE::self().alloc_sel();
    size_t len;

    // do we need to append to the file?
    if(write && _fileoff == inode->size) {
        OpenFiles::OpenFile *of = h.files().get_file(_ino);
        assert(of != nullptr);
        if(of->appending) {
            PRINT(this, "append already in progress");
            reply_error(is, Errors::EXISTS);
            return;
        }

        // continue in last extent, if there is space
        if(_extent > 0 && _fileoff == inode->size && (inode->size % h.sb().blocksize) != 0) {
            _extoff = inode->size % h.sb().blocksize;
            _extent--;
        }

        Extent e = {0 ,0};
        len = INodes::req_append(h, inode, _extent, sel, _oflags & MemGate::RWX, &e);
        if(Errors::occurred()) {
            PRINT(this, "append failed: " << Errors::to_string(Errors::last));
            reply_error(is, Errors::last);
            return;
        }

        _appending = true;
        _append_ext = e.length > 0 ? new Extent(e) : nullptr;
        of->appending = true;
    }
    else {
        // get next mem cap
        len = INodes::get_extent_mem(h, inode, _extent, _oflags & MemGate::RWX, sel);
        if(Errors::occurred()) {
            PRINT(this, "getting extent memory failed: " << Errors::to_string(Errors::last));
            reply_error(is, Errors::last);
            return;
        }
    }

    _lastoff = _extoff;
    _extlen = len;
    if(_extlen > 0) {
        // activate mem cap for client
        if(Syscalls::get().activate(_epcap, sel, 0) != Errors::NONE) {
            PRINT(this, "activate failed: " << Errors::to_string(Errors::last));
            reply_error(is, Errors::last);
            return;
        }

        // move forward
        _extent += 1;
        _extoff = 0;
        _fileoff += len - _lastoff;
    }
    else
        _lastoff = 0;

    PRINT(this, "file::" << (write ? "write" : "read")
        << " -> (" << _lastoff << ", " << (_extlen - _lastoff) << ")");

    if(h.revoke_first()) {
        // revoke last mem cap and remember new one
        if(_last != ObjCap::INVALID)
            VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _last, 1));
        _last = sel;

        reply_vmsg(is, Errors::NONE, _lastoff, _extlen - _lastoff);
    }
    else {
        reply_vmsg(is, Errors::NONE, _lastoff, _extlen - _lastoff);

        if(_last != ObjCap::INVALID)
            VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _last, 1));
        _last = sel;
    }
}

void M3FSFileSession::read(GateIStream &is) {
    read_write(is, false);
}

void M3FSFileSession::write(GateIStream &is) {
    read_write(is, true);
}

void M3FSFileSession::seek(GateIStream &is) {
    int whence;
    size_t off;
    is >> off >> whence;
    PRINT(this, "file::seek(path=" << _filename << ", off=" << off << ", whence=" << whence << ")");

    if(whence == M3FS_SEEK_CUR) {
        reply_error(is, Errors::INV_ARGS);
        return;
    }

    INode *inode = INodes::get(_meta->handle(), _ino);
    assert(inode != nullptr);

    size_t pos = INodes::seek(_meta->handle(), inode, off, whence, _extent, _extoff);
    _fileoff = pos + off;

    reply_vmsg(is, Errors::NONE, pos, off);
}

void M3FSFileSession::fstat(GateIStream &is) {
    PRINT(this, "file::fstat(path=" << _filename << ")");

    INode *inode = INodes::get(_meta->handle(), _ino);
    assert(inode != nullptr);

    m3::FileInfo info;
    INodes::stat(_meta->handle(), inode, info);

    reply_vmsg(is, Errors::NONE, info);
}

Errors::Code M3FSFileSession::commit(INode *inode, size_t submit) {
    assert(submit > 0);

    // were we actually appending?
    if(!_appending)
        return Errors::NONE;

    FSHandle &h = _meta->handle();

    // add new extent?
    if(_append_ext) {
        size_t blocks = (submit + h.sb().blocksize - 1) / h.sb().blocksize;
        size_t old_len = _append_ext->length;

        // append extent to file
        _append_ext->length = blocks;
        Errors::Code res = INodes::append_extent(h, inode, _append_ext);
        if(res != Errors::NONE)
            return res;

        // free superfluous blocks
        if(old_len > blocks)
            h.blocks().free(h, _append_ext->start + blocks, old_len - blocks);

        // for the position adjustment after the commit() call in read_write().
        _fileoff -= (_extlen - _lastoff) - submit;
        _extlen = blocks * h.sb().blocksize;
        _lastoff = 0;
        delete _append_ext;
    }

    // change size
    inode->size += submit;
    INodes::mark_dirty(h, inode->inode);

    // stop appending
    OpenFiles::OpenFile *ofile = h.files().get_file(_ino);
    assert(ofile != nullptr);
    assert(ofile->appending);
    ofile->appending = false;

    _append_ext = nullptr;
    _appending = false;
    return Errors::NONE;
}
