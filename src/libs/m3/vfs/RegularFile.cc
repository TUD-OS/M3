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

#include <base/util/Profile.h>
#include <base/log/Lib.h>

#include <m3/session/M3FS.h>
#include <m3/vfs/RegularFile.h>
#include <m3/vfs/MountTable.h>
#include <m3/vfs/VFS.h>

#include <limits>

namespace m3 {

bool RegularFile::ExtentCache::find(size_t off, uint16_t &ext, size_t &extoff) const {
    extoff = offset;
    for(size_t i = 0; i < locs.count(); ++i) {
        size_t len = locs.get_len(i);
        if(len == 0 || (off >= extoff && off < extoff + len)) {
            ext = i;
            return true;
        }
        extoff += len;
    }
    return false;
}

Errors::Code RegularFile::ExtentCache::request_next(Reference<M3FS> &sess, int fd,
                                                    bool writing, bool &extended) {
    // move forward
    offset += length;
    length = 0;
    first += locs.count();
    locs.clear();

    // get new locations
    uint flags = writing ? M3FS::EXTEND : 0;
    extended = sess->get_locs(fd, first, MAX_LOCS, locs, flags);
    if(Errors::last != Errors::NONE || locs.count() == 0)
        return Errors::last;

    // cache new length
    length = locs.total_length();

    return Errors::NONE;
}

RegularFile::RegularFile(int fd, Reference<M3FS> fs, int perms)
    : File(perms), _fd(fd), _fs(fs), _pos(), _cache(),
      _mem(MemGate::bind(ObjCap::INVALID)), _extended(), _max_write() {
    if(flags() & FILE_APPEND)
        seek(0, M3FS_SEEK_END);
}

RegularFile::~RegularFile() {
    // the fs-service will revoke it. so don't try to "unactivate" it afterwards
    _mem.rebind(ObjCap::INVALID);
    if(_fs.valid())
        _fs->close(_fd, _max_write.ext, _max_write.extoff);
}

Errors::Code RegularFile::stat(FileInfo &info) const {
    return const_cast<Reference<M3FS>&>(_fs)->fstat(_fd, info);
}

void RegularFile::set_pos(const Position &pos) {
    _pos = pos;

    // if our global extent has changed, we have to get new locations
    if(!_cache.contains_ext(pos.ext)) {
        _cache.invalidate();
        _cache.first = pos.ext;
        _cache.offset = pos.abs;
    }

    // update last write pos accordingly
    // TODO good idea?
    if(_extended && _pos.abs > _max_write.abs) {
        _max_write = _pos;
    }
}

ssize_t RegularFile::get_ext_len(bool writing, bool rebind) {
    if(!_cache.valid() || _cache.ext_len(_pos.ext) == 0) {
        Errors::Code res = _cache.request_next(_fs, _fd, writing, _extended);
        if(res != Errors::NONE)
            return res;
    }

    // don't read past the so far written part
    if(_extended && !writing && _pos.ext >= _max_write.ext) {
        if(_pos.ext > _max_write.ext || _pos.extoff >= _max_write.extoff)
            return 0;
        else
            return static_cast<ssize_t>(_max_write.extoff);
    }
    else {
        size_t len = _cache.ext_len(_pos.ext);

        if(rebind && len != 0 && _mem.sel() != _cache.sel(_pos.ext)) {
            _mem.rebind(_cache.sel(_pos.ext));
        }
        return static_cast<ssize_t>(len);
    }
}

ssize_t RegularFile::advance(size_t count, bool writing) {
    ssize_t extlen = get_ext_len(writing, true);
    if(extlen <= 0)
        return extlen;

    // determine next off and idx
    auto lastpos = _pos;
    size_t amount = _pos.advance(static_cast<size_t>(extlen), count);

    // remember the max. position we wrote to
    if(writing && lastpos.abs + amount > _max_write.abs) {
        _max_write = lastpos;
        _max_write.extoff += amount;
        _max_write.abs += amount;
    }

    return static_cast<ssize_t>(amount);
}

ssize_t RegularFile::read(void *buffer, size_t count) {
    if(~flags() & FILE_R)
        return Errors::NO_PERM;

    // determine the amount that we can read
    auto lastpos = _pos;
    ssize_t amount = advance(count, false);
    if(amount <= 0)
        return amount;

    LLOG(FS, "[" << _fd << "] read (" << fmt(amount, "#0x", 6) << ") @ (" << lastpos << ")");

    Profile::start(0xaaaa);
    _mem.read(buffer, static_cast<size_t>(amount), lastpos.extoff);
    Profile::stop(0xaaaa);

    return amount;
}

ssize_t RegularFile::write(const void *buffer, size_t count) {
    if(~flags() & FILE_W)
        return Errors::NO_PERM;

    // determine the amount that we can write
    auto lastpos = _pos;
    ssize_t amount = advance(count, true);
    if(amount <= 0)
        return amount;

    LLOG(FS, "[" << _fd << "] write (" << fmt(amount, "#0x", 6) << ") @ (" << lastpos << ")");

    // write to global memory
    Profile::start(0xaaaa);
    _mem.write(buffer, static_cast<size_t>(amount), lastpos.extoff);
    Profile::stop(0xaaaa);

    return amount;
}

Errors::Code RegularFile::read_next(capsel_t *memgate, size_t *offset, size_t *length) {
    if(~flags() & FILE_R)
        return Errors::NO_PERM;

    // determine the amount that we can read
    ssize_t extlen = get_ext_len(false, false);
    if(extlen <= 0)
        return static_cast<Errors::Code>(extlen);

    *memgate = _cache.sel(_pos.ext);
    *offset = _pos.extoff;
    *length = _pos.advance(static_cast<size_t>(extlen), std::numeric_limits<size_t>::max());
    return Errors::NONE;
}

Errors::Code RegularFile::begin_write(capsel_t *memgate, size_t *offset, size_t *length) {
    if(~flags() & FILE_W)
        return Errors::NO_PERM;

    ssize_t extlen = get_ext_len(true, false);
    if(extlen <= 0)
        return static_cast<Errors::Code>(extlen);

    *memgate = _cache.sel(_pos.ext);
    *offset = _pos.extoff;
    *length = static_cast<size_t>(extlen) - _pos.extoff;
    return Errors::NONE;
}

void RegularFile::commit_write(size_t length) {
    advance(length, true);
}

ssize_t RegularFile::seek(size_t off, int whence) {
    // simple cases first
    Position pos;
    if(whence == SEEK_CUR && off == 0) {
        pos = _pos;
    }
    // nothing to do if we want to seek to the beginning
    else if(!(whence == SEEK_SET && off == 0)) {
        if(whence == SEEK_CUR) {
            off += _pos.abs;
        }

        // is it already in our cache?
        if(whence != SEEK_END && _cache.contains_pos(off)) {
            uint16_t ext;
            size_t begin;
            // this is always successful, because we checked the range before
            _cache.find(off, ext, begin);

            pos = Position(_cache.first + ext, off - begin, off);
        }
        // otherwise, ask m3fs
        else {
            Errors::Code res = _fs->seek(_fd, off, whence, pos.ext, pos.extoff, pos.abs);
            if(res != Errors::NONE)
                return res;
        }
    }

    set_pos(pos);

    LLOG(FS, "[" << _fd << "] seek (" << fmt(off, "#0x", 6) << ", " << whence << ") -> " << _pos << ")");

    return static_cast<ssize_t>(_pos.abs);
}

size_t RegularFile::serialize_length() {
    return ostreamsize<int, int, size_t, bool, size_t, Position, uint16_t, size_t>();
}

void RegularFile::delegate(VPE &) {
    // nothing to do, because we let the child fetch new mem caps
}

void RegularFile::serialize(Marshaller &m) {
    size_t mid = VPE::self().mounts()->get_mount_id(&*_fs);
    m << _fd << flags() << mid << _extended << _pos << _max_write;
}

RegularFile *RegularFile::unserialize(Unmarshaller &um) {
    int fd, flags;
    size_t mid;
    um >> fd >> flags >> mid;

    Reference<M3FS> fs(static_cast<M3FS*>(VPE::self().mounts()->get_mount(mid)));
    RegularFile *file = new RegularFile(fd, fs, flags);
    um >> file->_extended >> file->_pos >> file->_max_write;
    return file;
}

}
