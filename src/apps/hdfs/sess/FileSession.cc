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
#include "../FSHandle.h"
#include <base/util/Time.h>

using namespace m3;

UsedBlocks::UsedBlocks(FSHandle &handle) : _handle(handle), used(0) {}

UsedBlocks::~UsedBlocks() {
    Time::start(0xfe00);
    for(size_t i = 0; i < used; i++) {
        _handle.metabuffer().quit(blocks[i]);
    }
    Time::stop(0xfe00);
}

void UsedBlocks::set(blockno_t bno) {
    blocks[used] = bno;
    used++;
}

void UsedBlocks::next() {
    used++;
}

void UsedBlocks::quit_last_n(size_t n) {
    for(size_t i = 0; i<n; i++) {
        if(used != 0) {
            _handle.metabuffer().quit(blocks[used-1]);
            used--;
        }
    }
}

M3FSFileSession::M3FSFileSession(capsel_t srv_sel, M3FSMetaSession *meta,
                                 const m3::String &filename, int flags, m3::inodeno_t ino)
    : M3FSSession(srv_sel, srv_sel == ObjCap::INVALID ? srv_sel : m3::VPE::self().alloc_sels(2)),
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
    _meta->handle().files().add_sess(this);
}

M3FSFileSession::~M3FSFileSession() {
    PRINT(this, "file::close(path=" << _filename << ")");

    delete _sgate;

    if(_append_ext) {
        FSHandle &h = _meta->handle();
        h.blocks().free(h, _append_ext->start, _append_ext->length);
        delete _append_ext;
    }

    _meta->handle().files().rem_sess(this);
    _meta->remove_file(this);

    if(_last != ObjCap::INVALID) {
        cout << "#!invalid: " << _last << "\n";
        VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _last, 1));
    }
    cout << "#1\n";
}

Errors::Code M3FSFileSession::clone(capsel_t srv, KIF::Service::ExchangeData &data) {
    PRINT(this, "file::clone(path=" << _filename << ")");

    auto nfile =  new M3FSFileSession(srv, _meta, _filename, _oflags, _ino);

    data.args.count = 0;
    data.caps = nfile->caps().value();

    return Errors::NONE;
}

Errors::Code M3FSFileSession::get_mem(KIF::Service::ExchangeData &data) {
    EVENT_TRACER_FS_getlocs();
    if(data.args.count != 1)
        return Errors::INV_ARGS;

    size_t offset = data.args.vals[0];


    UsedBlocks used_blocks = UsedBlocks(_meta->handle());

    PRINT(this, "file::get_mem(path=" << _filename << ", offset=" << offset << ")");

    INode *inode = INodes::get(_meta->handle(), _ino, &used_blocks);
    assert(inode != nullptr);

    // determine extent from byte offset
    size_t firstOff = offset;
    {
        size_t tmp_extent, tmp_extoff;
        INodes::seek(_meta->handle(), inode, firstOff, M3FS_SEEK_SET, tmp_extent, tmp_extoff, &used_blocks);
        offset = tmp_extent;
    }

    capsel_t sel = VPE::self().alloc_sel();
    Errors::last = Errors::NONE;
    size_t extlen = 0;
    size_t len = INodes::get_extent_mem(_meta->handle(), inode, offset, 0, &extlen,
                                        _oflags & MemGate::RWX, sel, true, &used_blocks, _accessed);
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

void M3FSFileSession::next_in_out(GateIStream &is, bool out) {
    UsedBlocks used_blocks = UsedBlocks(_meta->handle());

    PRINT(this, "file::next_" << (out ? "out" : "in") << "(); "
        << "file[path=" << _filename << ", fileoff=" << _fileoff << ", ext=" << _extent
        << ", extoff=" << _extoff << "]");

    if((out && !(_oflags & FILE_W)) || (!out && !(_oflags & FILE_R))) {
        reply_error(is, Errors::NO_PERM);
        return;
    }

    FSHandle &h = _meta->handle();
    INode *inode = INodes::get(h, _ino, &used_blocks);
    assert(inode != nullptr);

    // in/out implicitly commits the previous in/out request
    if(out && _appending) {
        Errors::Code res = commit(inode, _lastbytes - _lastoff % h.sb().blocksize, &used_blocks);
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
        OpenFiles::OpenFile *of = h.files().get_file(_ino);
        assert(of != nullptr);
        if(of->appending) {
            PRINT(this, "append already in progress");
            reply_error(is, Errors::EXISTS);
            return;
        }

        // continue in last extent, if there is space
        if(_extent > 0 && _fileoff == inode->size && (_fileoff % h.sb().blocksize) != 0) {
            size_t off = 0;
            _fileoff = INodes::seek(h, inode, off, M3FS_SEEK_END, _extent, _extoff, &used_blocks);
        }

        Extent e = {0 ,0};
        len = INodes::req_append(h, inode, _extent, _extoff, &extlen, sel, _oflags & MemGate::RWX, &e, &used_blocks, _accessed);
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
        len = INodes::get_extent_mem(h, inode, _extent, _extoff, &extlen, _oflags & MemGate::RWX, sel, out, &used_blocks, _accessed);
        if(Errors::occurred()) {
            PRINT(this, "getting extent memory failed: " << Errors::to_string(Errors::last));
            reply_error(is, Errors::last);
            return;
        }
    }

    _lastoff = _extoff;
    _extlen = extlen;
    _lastbytes = len - _lastoff % h.sb().blocksize;
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
            _extoff += len - _extoff % h.sb().blocksize;
            _moved_forward = false;
        }
        _fileoff += len - _lastoff % h.sb().blocksize;
    }
    else {
        _lastoff = 0;
        sel = ObjCap::INVALID;
    }

    PRINT(this, "file::next_" << (out ? "out" : "in")
        << "() -> (" << _lastoff << ", " << _lastbytes << ")");

    if(h.revoke_first()) {
        // revoke last mem cap and remember new one
        if(_last != ObjCap::INVALID)
            VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _last, 1));
        _last = sel;

        reply_vmsg(is, Errors::NONE, _lastoff % h.sb().blocksize, _lastbytes );
    }
    else {
        reply_vmsg(is, Errors::NONE, _lastoff % h.sb().blocksize, _lastbytes );

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

    UsedBlocks used_blocks = UsedBlocks(_meta->handle());

    PRINT(this, "file::commit(nbytes=" << nbytes << "); "
        << "file[path=" << _filename << ", fileoff=" << _fileoff << ", ext=" << _extent
        << ", extoff=" << _extoff << "]");

    if(nbytes == 0 || nbytes > _lastbytes) {
        reply_error(is, Errors::INV_ARGS);
        return;
    }

    FSHandle &h = _meta->handle();
    INode *inode = INodes::get(h, _ino, &used_blocks);
    assert(inode != nullptr);

    Errors::Code res;
    if(_appending)
        res = commit(inode, nbytes, &used_blocks);
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


    UsedBlocks used_blocks = UsedBlocks(_meta->handle());

    PRINT(this, "file::seek(path=" << _filename << ", off=" << off << ", whence=" << whence << ")");

    if(whence == M3FS_SEEK_CUR) {
        reply_error(is, Errors::INV_ARGS);
        return;
    }

    INode *inode = INodes::get(_meta->handle(), _ino, &used_blocks);
    assert(inode != nullptr);

    size_t pos = INodes::seek(_meta->handle(), inode, off, whence, _extent, _extoff, &used_blocks);
    _fileoff = pos + off;


    reply_vmsg(is, Errors::NONE, pos, off);
}

void M3FSFileSession::fstat(GateIStream &is) {


    UsedBlocks used_blocks = UsedBlocks(_meta->handle());

    PRINT(this, "file::fstat(path=" << _filename << ")");

    INode *inode = INodes::get(_meta->handle(), _ino, &used_blocks);
    assert(inode != nullptr);

    m3::FileInfo info;
    INodes::stat(_meta->handle(), inode, info);


    reply_vmsg(is, Errors::NONE, info);
}

Errors::Code M3FSFileSession::commit(INode *inode, size_t submit, UsedBlocks *used_blocks) {
    assert(submit > 0);

    // were we actually appending?
    if(!_appending)
        return Errors::NONE;

    FSHandle &h = _meta->handle();

    // adjust file position.
    _fileoff -= (_lastbytes - _lastoff % h.sb().blocksize) - submit;

    // add new extent?
    size_t lastoff = _lastoff;
    bool truncated = _lastoff + submit < _lastbytes; // TODO submit from the offset or the first given block?
    size_t prev_ext_len = 0;
    if(_append_ext) {
        size_t blocks = (submit + h.sb().blocksize - 1) / h.sb().blocksize;
        size_t old_len = _append_ext->length;

        // append extent to file
        _append_ext->length = blocks;
        Errors::Code res = INodes::append_extent(h, inode, _append_ext, &prev_ext_len, used_blocks);
        if(res != Errors::NONE)
            return res;

        // free superfluous blocks
        if(old_len > blocks)
            h.blocks().free(h, _append_ext->start + blocks, old_len - blocks);

        _extlen = blocks * h.sb().blocksize;
        _extoff = 0;
        // have we appended the new extent to the previous extent?
        if(prev_ext_len > 0)
            _extent--;
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
        _extoff = prev_ext_len + lastoff + submit; // TODO +lastoff?
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
