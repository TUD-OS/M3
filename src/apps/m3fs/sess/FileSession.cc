/*
 * Copyright (C) 2018, Sebastian Reimers <sebastian.reimers@mailbox.tu-dresden.de>
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

#include "FileSession.h"

#include <base/util/Time.h>

#include <m3/Syscalls.h>
#include <m3/session/M3FS.h>

#include "../FSHandle.h"
#include "../data/INodes.h"
#include "MetaSession.h"

using namespace m3;

M3FSFileSession::M3FSFileSession(FSHandle &handle, capsel_t srv_sel, M3FSMetaSession *meta,
                                 const m3::String &filename, int flags, m3::inodeno_t ino)
    : M3FSSession(handle, srv_sel, srv_sel == ObjCap::INVALID ? srv_sel : m3::VPE::self().alloc_sels(2)),
      m3::SListItem(),
      _extent(),
      _extoff(),
      _lastoff(),
      _extlen(),
      _fileoff(),
      _lastbytes(),
      _accessed(),
      _moved_forward(false),
      _appending(),
      _append_ext(),
      _last(ObjCap::INVALID),
      _epcap(ObjCap::INVALID),
      _sgate(srv_sel == ObjCap::INVALID
        ? nullptr
        : new m3::SendGate(m3::SendGate::create(&meta->rgate(), reinterpret_cast<label_t>(this),
                                                MSG_SIZE, nullptr, sel() + 1))),
      _oflags(flags),
      _filename(filename),
      _ino(ino),
      _capscon(),
      _meta(meta) {
    hdl().files().add_sess(this);
}

M3FSFileSession::~M3FSFileSession() {
    PRINT(this, "file::close(path=" << _filename << ")");

    Request r(hdl());

    delete _sgate;

    if(_append_ext) {
        hdl().blocks().free(r, _append_ext->start, _append_ext->length);
        delete _append_ext;
    }

    hdl().files().rem_sess(this);
    _meta->remove_file(this);

    if(_last != ObjCap::INVALID)
        VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _last, 1));
}

Errors::Code M3FSFileSession::clone(capsel_t srv, KIF::Service::ExchangeData &data) {
    PRINT(this, "file::clone(path=" << _filename << ")");

    auto nfile =  new M3FSFileSession(hdl(), srv, _meta, _filename, _oflags, _ino);

    data.args.count = 0;
    data.caps = nfile->caps().value();

    return Errors::NONE;
}

Errors::Code M3FSFileSession::get_mem(KIF::Service::ExchangeData &data) {
    EVENT_TRACER_FS_getlocs();
    if(data.args.count != 1)
        return Errors::INV_ARGS;

    size_t offset = data.args.vals[0];

    Request r(hdl());

    PRINT(this, "file::get_mem(path=" << _filename << ", offset=" << offset << ")");

    INode *inode = INodes::get(r, _ino);
    assert(inode != nullptr);

    // determine extent from byte offset
    size_t firstOff = offset;
    size_t ext_off;
    {
        size_t tmp_extent;
        INodes::seek(r, inode, firstOff, M3FS_SEEK_SET, tmp_extent, ext_off);
        offset = tmp_extent;
    }

    capsel_t sel = VPE::self().alloc_sel();
    Errors::last = Errors::NONE;
    size_t extlen = 0;
    size_t len = INodes::get_extent_mem(r, inode, offset, ext_off, &extlen,
                                        _oflags & MemGate::RWX, sel, true, _accessed);
    if(Errors::occurred()) {
        PRINT(this, "getting extent memory failed: " << Errors::to_string(Errors::last));

        return Errors::last;
    }

    data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel, 1).value();
    data.args.count = 2;
    data.args.vals[0] = 0;
    data.args.vals[1] = len;

    PRINT(this, "file::get_mem -> " << len);

    _capscon.add(sel);

    return Errors::NONE;
}

void M3FSFileSession::next_in_out(GateIStream &is, bool out) {
    PRINT(this, "file::next_" << (out ? "out" : "in") << "(); "
                              << "file[path=" << _filename << ", fileoff=" << _fileoff << ", ext=" << _extent
                              << ", extoff=" << _extoff << "]");

    if((out && !(_oflags & FILE_W)) || (!out && !(_oflags & FILE_R))) {
        reply_error(is, Errors::NO_PERM);
        return;
    }

    Request r(hdl());
    INode *inode = INodes::get(r, _ino);
    assert(inode != nullptr);

    // in/out implicitly commits the previous in/out request
    if(out && _appending) {
        Errors::Code res = commit(r, inode, _lastbytes);
        if(res != Errors::NONE) {
            reply_error(is, res);
            return;
        }
    }

    if(_accessed < 31)
        _accessed++;

    Errors::last = Errors::NONE;
    capsel_t sel = VPE::self().alloc_sel();
    size_t len;
    size_t extlen = 0;

    // do we need to append to the file?
    if(out && _fileoff == inode->size) {
        OpenFiles::OpenFile *of = hdl().files().get_file(_ino);
        assert(of != nullptr);
        if(of->appending) {
            PRINT(this, "append already in progress");
            reply_error(is, Errors::EXISTS);
            return;
        }

        // continue in last extent, if there is space
        if(_extent > 0 && _fileoff == inode->size && (_fileoff % hdl().sb().blocksize) != 0) {
            size_t off = 0;
            _fileoff = INodes::seek(r, inode, off, M3FS_SEEK_END, _extent, _extoff);
        }

        Extent e = {0, 0};
        len = INodes::req_append(r, inode, _extent, _extoff, &extlen, sel,
                                 _oflags & MemGate::RWX, &e, _accessed);
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
        len = INodes::get_extent_mem(r, inode, _extent, _extoff, &extlen,
                                     _oflags & MemGate::RWX, sel, out, _accessed);
        if(Errors::occurred()) {
            PRINT(this, "getting extent memory failed: " << Errors::to_string(Errors::last));
            reply_error(is, Errors::last);
            return;
        }
    }

    _lastoff = _extoff;
    // the mem cap covers all blocks from <_extoff> to <_extoff>+<len>. thus, the offset to start
    // is the offset within the first of these blocks.
    size_t capoff = _lastoff % hdl().sb().blocksize;
    _extlen = extlen;
    _lastbytes = len - capoff;
    if(len > 0) {
        // activate mem cap for client
        if(Syscalls::get().activate(_epcap, sel, 0) != Errors::NONE) {
            PRINT(this, "activate failed: " << Errors::to_string(Errors::last));
            reply_error(is, Errors::last);
            return;
        }

        // move forward
        if(_extoff + len >= _extlen) {
            _moved_forward = true;
            _extent += 1;
            _extoff = 0;
        }
        else {
            _extoff += len - _extoff % hdl().sb().blocksize;
            _moved_forward = false;
        }
        _fileoff += len - capoff;
    }
    else {
        capoff = _lastoff = 0;
        sel = ObjCap::INVALID;
    }

    PRINT(this, "file::next_" << (out ? "out" : "in")
                              << "() -> (" << _lastoff << ", " << _lastbytes << ")");

    if(hdl().revoke_first()) {
        // revoke last mem cap and remember new one
        if(_last != ObjCap::INVALID)
            VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _last, 1));
        _last = sel;

        reply_vmsg(is, Errors::NONE, capoff, _lastbytes);
    }
    else {
        reply_vmsg(is, Errors::NONE, capoff, _lastbytes);

        if(_last != ObjCap::INVALID)
            VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _last, 1));
        _last = sel;
    }
}

void M3FSFileSession::next_in(GateIStream &is) {
    next_in_out(is, false);
}

void M3FSFileSession::next_out(GateIStream &is) {
    next_in_out(is, true);
}

void M3FSFileSession::commit(GateIStream &is) {
    size_t nbytes;
    is >> nbytes;

    Request r(hdl());

    PRINT(this, "file::commit(nbytes=" << nbytes << "); "
                                       << "file[path=" << _filename << ", fileoff=" << _fileoff
                                       << ", ext=" << _extent << ", extoff=" << _extoff << "]");

    if(nbytes == 0 || nbytes > _lastbytes) {
        reply_error(is, Errors::INV_ARGS);
        return;
    }

    INode *inode = INodes::get(r, _ino);
    assert(inode != nullptr);

    Errors::Code res;
    if(_appending)
        res = commit(r, inode, nbytes);
    else {
        res = Errors::NONE;
        if(_moved_forward && _lastoff + nbytes < _extlen)
            _extent--;
        if(nbytes < _lastbytes)
            _extoff = _lastoff + nbytes;
    }
    _lastbytes = 0;

    reply_vmsg(is, res, inode->size);
}

void M3FSFileSession::seek(GateIStream &is) {
    int whence;
    size_t off;
    is >> off >> whence;

    Request r(hdl());

    PRINT(this, "file::seek(path=" << _filename << ", off=" << off << ", whence=" << whence << ")");

    if(whence == M3FS_SEEK_CUR) {
        reply_error(is, Errors::INV_ARGS);
        return;
    }

    INode *inode = INodes::get(r, _ino);
    assert(inode != nullptr);

    size_t pos = INodes::seek(r, inode, off, whence, _extent, _extoff);
    _fileoff = pos + off;

    reply_vmsg(is, Errors::NONE, pos, off);
}

void M3FSFileSession::fstat(GateIStream &is) {
    Request r(hdl());

    PRINT(this, "file::fstat(path=" << _filename << ")");

    INode *inode = INodes::get(r, _ino);
    assert(inode != nullptr);

    m3::FileInfo info;
    INodes::stat(r, inode, info);

    reply_vmsg(is, Errors::NONE, info);
}

Errors::Code M3FSFileSession::commit(Request &r, INode *inode, size_t submit) {
    assert(submit > 0);

    // were we actually appending?
    if(!_appending)
        return Errors::NONE;

    // adjust file position.
    _fileoff -= _lastbytes - submit;

    // add new extent?
    size_t lastoff = _lastoff;
    bool truncated = submit < _lastbytes;
    size_t prev_ext_len = 0;
    if(_append_ext) {
        uint32_t blocksize = r.hdl().sb().blocksize;
        size_t blocks = (submit + blocksize - 1) / blocksize;
        size_t old_len = _append_ext->length;

        // append extent to file
        _append_ext->length = blocks;
        Errors::Code res = INodes::append_extent(r, inode, _append_ext, &prev_ext_len);
        if(res != Errors::NONE)
            return res;

        // free superfluous blocks
        if(old_len > blocks)
            r.hdl().blocks().free(r, _append_ext->start + blocks, old_len - blocks);

        _extlen = blocks * blocksize;
        // have we appended the new extent to the previous extent?
        if(prev_ext_len > 0)
            _extent--;
        _lastoff = 0;
        delete _append_ext;
    }

    // we did not get the whole extent, but truncated it, so we have to move forward
    if(!_moved_forward) {
        _extent++;
        _extoff = 0;
    }
    else if(truncated) {
        // move to the end of the last extent
        _extent--;
        _extoff = prev_ext_len + lastoff + submit;
    }

    // change size
    inode->size += submit;
    INodes::mark_dirty(r, inode->inode);

    // stop appending
    OpenFiles::OpenFile *ofile = r.hdl().files().get_file(_ino);
    assert(ofile != nullptr);
    assert(ofile->appending);
    ofile->appending = false;

    _append_ext = nullptr;
    _appending = false;
    return Errors::NONE;
}
