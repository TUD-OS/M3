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

#include <m3/session/M3FS.h>
#include <m3/Syscalls.h>

#include "../data/INodes.h"
#include "FileSession.h"
#include "MetaSession.h"

using namespace m3;

M3FSFileSession::M3FSFileSession(capsel_t srv, M3FSMetaSession *meta, const m3::String &filename,
                                 int flags, const m3::INode &inode)
    : M3FSSession(),
      m3::SListItem(),
      _extent(),
      _extoff(),
      _lastoff(),
      _extlen(),
      _filename(filename),
      _epcap(ObjCap::INVALID),
      _sess(m3::VPE::self().alloc_sels(2)),
      _sgate(m3::SendGate::create(&meta->rgate(), reinterpret_cast<label_t>(this),
                                  MSG_SIZE, nullptr, _sess + 1)),
      _oflags(flags),
      _xstate(TransactionState::NONE),
      _inode(inode),
      _last(ObjCap::INVALID),
      _capscon(),
      _meta(meta) {
    Syscalls::get().createsessat(_sess, srv, reinterpret_cast<word_t>(this));

    _meta->handle().files().add_sess(this);
}

M3FSFileSession::~M3FSFileSession() {
    SLOG(FS, fmt((word_t)this, "#x") << ": file::close(path=" << _filename << ")");

    _meta->handle().files().rem_sess(this);
    _meta->remove_file(this);

    if(_last != ObjCap::INVALID)
        VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _last, 1));
}

Errors::Code M3FSFileSession::clone(capsel_t srv, KIF::Service::ExchangeData &data) {
    SLOG(FS, fmt((word_t)this, "#x") << ": file::clone(path=" << _filename << ")");

    auto nfile =  new M3FSFileSession(srv, _meta, _filename, _oflags, _inode);

    data.args.count = 0;
    data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, nfile->_sess, 2).value();

    return Errors::NONE;
}

Errors::Code M3FSFileSession::get_mem(KIF::Service::ExchangeData &data) {
    EVENT_TRACER_FS_getlocs();
    if(data.args.count != 1)
        return Errors::INV_ARGS;

    size_t offset = data.args.vals[0];

    SLOG(FS, fmt((word_t)this, "#x") << ": file::get_mem(path=" << _filename
        << ", offset=" << offset << ")");

    // determine extent from byte offset
    size_t firstOff = offset;
    {
        size_t tmp_extent, tmp_extoff;
        INodes::seek(_meta->handle(), &_inode, firstOff, M3FS_SEEK_SET, tmp_extent, tmp_extoff);
        offset = tmp_extent;
    }

    KIF::CapRngDesc crd;
    Errors::last = Errors::NONE;
    loclist_type *locs = INodes::get_locs(_meta->handle(), &_inode, offset, 1,
                                          0, _oflags & MemGate::RWX, crd);
    if(!locs) {
        SLOG(FS, fmt((word_t)this, "#x") << ": Determining locations failed: "
            << Errors::to_string(Errors::last));
        return Errors::last;
    }

    data.caps = crd.value();
    data.args.count = 2;
    data.args.vals[0] = firstOff;
    data.args.vals[1] = locs->get_len(0);

    SLOG(FS, "Received cap: " << locs->get_len(0));

    _capscon.add(crd);
    return Errors::NONE;
}

void M3FSFileSession::read_write(GateIStream &is, bool write) {
    size_t submit;
    is >> submit;

    SLOG(FS, fmt((word_t)this, "#x")
        << ": file::" << (write ? "write" : "read") << "(submit=" << submit << "); "
        << "file[path=" << _filename << ", extent=" << _extent << ", extoff=" << _extoff << "]");

    if((write && !(_oflags & FILE_W)) || (!write && !(_oflags & FILE_R))) {
        reply_error(is, Errors::NO_PERM);
        return;
    }

    if(submit > 0) {
        if(_extent == 0) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        if(_lastoff + submit < _extlen) {
            _extent--;
            _extoff = _lastoff + submit;
        }
        Errors::Code res = write ? commit(_extent, _extoff) : Errors::NONE;
        reply_vmsg(is, res, _inode.size);
        return;
    }

    // get next mem cap
    KIF::CapRngDesc crd;
    size_t old_ino_size = _inode.size;
    Errors::last = Errors::NONE;
    loclist_type *locs = INodes::get_locs(_meta->handle(), &_inode, _extent, 1,
                                          write ? _meta->handle().extend() : 0,
                                          _oflags & MemGate::RWX, crd);
    if(!locs) {
        SLOG(FS, fmt((word_t)this, "#x") << ": Determining locations failed: "
            << Errors::to_string(Errors::last));
        reply_error(is, Errors::last);
        return;
    }

    _lastoff = _extoff;
    _extlen = locs->get_len(0);
    if(_extlen > 0) {
        // activate mem cap for client
        if(Syscalls::get().activate(_epcap, crd.start(), 0) != Errors::NONE) {
            SLOG(FS, fmt((word_t)this, "#x") << ": activate failed: "
                << Errors::to_string(Errors::last));
            reply_error(is, Errors::last);
            return;
        }

        // move forward
        _extent += 1;
        _extoff = 0;
    }

    // start/continue transaction
    if(_inode.size > old_ino_size && _xstate != TransactionState::ABORTED)
        _xstate = TransactionState::OPEN;

    SLOG(FS, fmt((word_t)this, "#x")
        << ": file::" << (write ? "write" : "read")
        << " -> (" << _lastoff << ", " << (_extlen - _lastoff) << ")");

    // reply first
    reply_vmsg(is, Errors::NONE, _lastoff, _extlen - _lastoff);

    // revoke last mem cap and remember new one
    if(_last != ObjCap::INVALID)
        VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _last, 1));
    _last = crd.start();
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
    SLOG(FS, fmt((word_t)this, "#x") << ": file::seek(path="
        << _filename << ", off=" << off << ", whence=" << whence << ")");

    if(whence == SEEK_CUR) {
        reply_error(is, Errors::INV_ARGS);
        return;
    }

    size_t pos = INodes::seek(_meta->handle(), &_inode, off, whence, _extent, _extoff);
    reply_vmsg(is, Errors::NONE, pos, off);
}

void M3FSFileSession::fstat(GateIStream &is) {
    SLOG(FS, fmt((word_t)this, "#x") << ": file::fstat(path=" << _filename << ")");

    m3::FileInfo info;
    INodes::stat(_meta->handle(), &_inode, info);
    reply_vmsg(is, Errors::NONE, info);
}

Errors::Code M3FSFileSession::commit(size_t extent, size_t extoff) {
    if(_xstate == TransactionState::ABORTED)
        return Errors::COMMIT_FAILED;

    // have we increased the filesize?
    m3::INode *ninode = INodes::get(_meta->handle(), _inode.inode);
    if(_inode.size > ninode->size) {
        // get the old offset within the last extent
        size_t orgoff = 0;
        if(ninode->extents > 0) {
            Extent *indir = nullptr;
            Extent *ch = INodes::get_extent(_meta->handle(), ninode, ninode->extents - 1, &indir, false);
            assert(ch != nullptr);
            orgoff = ch->length * _meta->handle().sb().blocksize;
            size_t mod;
            if(((mod = ninode->size % _meta->handle().sb().blocksize)) > 0)
                orgoff -= _meta->handle().sb().blocksize - mod;
        }

        // then cut it to either the org size or the max. position we've written to,
        // whatever is bigger
        if(ninode->extents == 0 || extent > ninode->extents - 1 ||
           (extent == ninode->extents - 1 && extoff > orgoff)) {
            INodes::truncate(_meta->handle(), &_inode, _extent, _extoff);
        }
        else {
            INodes::truncate(_meta->handle(), &_inode, ninode->extents - 1, orgoff);
            _inode.size = ninode->size;
        }
        memcpy(ninode, &_inode, sizeof(*ninode));

        // update the inode in all open files
        // and let all future commits for this file fail
        OpenFiles::OpenFile *ofile = _meta->handle().files().get_file(_inode.inode);
        assert(ofile != nullptr);
        for(auto s = ofile->sessions.begin(); s != ofile->sessions.end(); ++s) {
            if(&*s != this && s->_xstate == TransactionState::OPEN) {
                memcpy(&s->_inode, ninode, sizeof(*ninode));
                s->_xstate = TransactionState::ABORTED;
                // TODO revoke access, if necessary
            }
        }
    }

    _xstate = TransactionState::NONE;

    return Errors::NONE;
}
