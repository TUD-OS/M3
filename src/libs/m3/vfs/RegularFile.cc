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

RegularFile::RegularFile(int fd, Reference<M3FS> fs, int perms)
    : File(perms), _fd(fd), _extended(), _begin(), _length(), _pos(),
      /* pass an arbitrary selector first */
      _memcaps(), _locs(), _lastmem(MemGate::bind(0)), _last_extent(0), _last_off(0),
      _fs(fs) {
    if(flags() & FILE_APPEND)
        seek(0, M3FS_SEEK_END);
}

RegularFile::~RegularFile() {
    // the fs-service will revoke it. so don't try to "unactivate" it afterwards
    _lastmem.rebind(ObjCap::INVALID);
    if(_fs.valid())
        _fs->close(_fd, _last_extent, _last_off);
}

Errors::Code RegularFile::stat(FileInfo &info) const {
    return const_cast<Reference<M3FS>&>(_fs)->fstat(_fd, info);
}

void RegularFile::adjust_written_part() {
    // allow seeks beyond the so far written part
    // TODO actually, we should also append to the file, if necessary
    if(_extended && (_pos.global > _last_extent || (_pos.global == _last_extent && _pos.offset > _last_off))) {
        _last_extent = _pos.global;
        _last_off = _pos.offset;
    }
}

ssize_t RegularFile::seek(size_t off, int whence) {
    size_t global, extoff;
    size_t pos;

    // seek to beginning?
    if(whence == M3FS_SEEK_SET && off == 0) {
        global = 0;
        extoff = 0;
        pos = 0;
    }
    // is it already in our local data?
    // TODO we could support that for SEEK_CUR as well
    else if(whence == M3FS_SEEK_SET && _pos.valid() && off >= _begin && off < _begin + _length) {
        size_t begin = _begin;
        for(size_t i = 0; i < MAX_LOCS; ++i) {
            size_t len = _locs.get(i);
            if(!len || (off >= begin && off < begin + len)) {
                _pos.global += i - _pos.local;
                _pos.local = i;
                _pos.offset = off - begin;
                adjust_written_part();
                return static_cast<ssize_t>(off);
            }
            begin += len;
        }
        UNREACHED;
    }
    else {
        global = _pos.global;
        extoff = _pos.offset;
        _fs->seek(_fd, off, whence, global, extoff, pos);
    }

    // if our global extent has changed, we have to get new locations
    if(_pos.global != global) {
        _pos.global = global;
        _pos.local = MAX_LOCS;
        // only in this case, we have to reset our start-pos
        _begin = pos;
        // we don't have locations yet
        _length = 0;
    }
    _pos.offset = extoff;
    adjust_written_part();

    LLOG(FS, "[" << _fd << "] seek (" << fmt(off, "#0x", 6) << ", " << whence << ") -> ("
        << fmt(_pos.global, 2) << ", " << fmt(_pos.offset, "#0x", 6) << ")");
    return static_cast<ssize_t>(pos);
}

size_t RegularFile::get_amount(size_t extlen, size_t count, Position &pos) const {
    // determine next off and idx
    size_t amount;
    if(count >= extlen - pos.offset) {
        amount = extlen - pos.offset;
        pos.next_extent();
    }
    else {
        amount = count;
        pos.offset += amount;
    }
    return amount;
}

ssize_t RegularFile::read(void *buffer, size_t count) {
    if(~flags() & FILE_R)
        return Errors::NO_PERM;

    char *buf = reinterpret_cast<char*>(buffer);
    while(count > 0) {
        // figure out where that part of the file is in memory, based on our location db
        ssize_t extlen = get_location(_pos, false, true);
        if(extlen < 0)
            return extlen;
        if(extlen == 0)
            break;

        // determine next off and idx
        size_t memoff = _pos.offset;
        size_t amount = get_amount(static_cast<size_t>(extlen), count, _pos);

        LLOG(FS, "[" << _fd << "] read (" << fmt(amount, "#0x", 6) << ") -> ("
            << fmt(_pos.global, 2) << ", " << fmt(_pos.offset, "#0x", 6) << ")");

        // read from global memory
        Profile::start(0xaaaa);
        _lastmem.read(buf, amount, memoff);
        Profile::stop(0xaaaa);
        buf += amount;
        count -= amount;
    }
    return buf - reinterpret_cast<char*>(buffer);
}

ssize_t RegularFile::write(const void *buffer, size_t count) {
    if(~flags() & FILE_W)
        return Errors::NO_PERM;

    const char *buf = reinterpret_cast<const char*>(buffer);
    while(count > 0) {
        // figure out where that part of the file is in memory, based on our location db
        ssize_t extlen = get_location(_pos, true, true);
        if(extlen < 0)
            return extlen;
        if(extlen == 0)
            break;

        // determine next off and idx
        uint16_t lastglobal = _pos.global;
        size_t memoff = _pos.offset;
        size_t amount = get_amount(static_cast<size_t>(extlen), count, _pos);

        // remember the max. position we wrote to
        if(lastglobal >= _last_extent) {
            if(lastglobal > _last_extent || memoff + amount > _last_off)
                _last_off = memoff + amount;
            _last_extent = lastglobal;
        }

        LLOG(FS, "[" << _fd << "] write(" << fmt(amount, "#0x", 6) << ") -> ("
            << fmt(_pos.global, 2) << ", " << fmt(_pos.offset, "0", 6) << ")");

        // write to global memory
        Profile::start(0xaaaa);
        _lastmem.write(buf, amount, memoff);
        Profile::stop(0xaaaa);
        buf += amount;
        count -= amount;
    }
    return buf - reinterpret_cast<const char*>(buffer);
}

Errors::Code RegularFile::read_next(capsel_t *memgate, size_t *offset, size_t *length) {
    if(~flags() & FILE_R)
        return Errors::NO_PERM;

    // figure out where that part of the file is in memory, based on our location db
    ssize_t extlen = get_location(_pos, false, false);
    if(extlen < 0)
        return Errors::last;

    *memgate = _memcaps.start() + _pos.local;
    *offset = _pos.offset;
    if(extlen == 0)
        *length = 0;
    else
        *length = get_amount(static_cast<size_t>(extlen), std::numeric_limits<size_t>::max(), _pos);
    return Errors::NONE;
}

Errors::Code RegularFile::begin_write(capsel_t *memgate, size_t *offset, size_t *length) {
    if(~flags() & FILE_W)
        return Errors::NO_PERM;

    // figure out where that part of the file is in memory, based on our location db
    ssize_t extlen = get_location(_pos, true, false);
    if(extlen < 0)
        return Errors::last;

    *memgate = _memcaps.start() + _pos.local;
    *offset = _pos.offset;
    *length = static_cast<size_t>(extlen) - _pos.offset;
    return Errors::NONE;
}

void RegularFile::commit_write(size_t length) {
    uint16_t lastglobal = _pos.global;
    size_t extlen = _locs.get(_pos.local);
    size_t offset = _pos.offset;
    get_amount(extlen, length, _pos);

    // remember the max. position we wrote to
    if(lastglobal >= _last_extent) {
        if(lastglobal > _last_extent || offset + length > _last_off)
            _last_off = offset + length;
        _last_extent = lastglobal;
    }
}

ssize_t RegularFile::get_location(Position &pos, bool writing, bool rebind) const {
    if(!pos.valid() || (writing && _locs.get(pos.local) == 0)) {
        _locs.clear();

        // move forward
        _begin += _length;
        _length = 0;

        // get new locations
        pos.local = 0;
        // TODO it would be better to increment the number of blocks we create, like start with
        // 4, then 8, then 16, up to a certain limit.
        _extended |= const_cast<Reference<M3FS>&>(_fs)->get_locs(_fd, pos.global, MAX_LOCS,
            _memcaps, _locs, writing ? M3FS::EXTEND : 0);
        if(Errors::last != Errors::NONE || _locs.count() == 0)
            return Errors::last;

        // determine new length
        for(size_t i = 0; i < _locs.count(); ++i)
            _length += _locs.get(i);

        // when seeking to the end, we might be already at the end of a extent. if that happened,
        // go to the beginning of the next one
        size_t length = _locs.get(0);
        if(pos.offset == length)
            pos.next_extent();
        if(rebind)
            _lastmem.rebind(_memcaps.start() + pos.local);
        return static_cast<ssize_t>(_locs.get(pos.local));
    }
    else {
        // don't read past the so far written part
        if(_extended && !writing && pos.global + pos.local >= _last_extent) {
            if(pos.global + pos.local > _last_extent)
                return 0;
            // take care that there is at least something to read; if not, break here to not advance
            // to the next extent (see get_amount).
            if(pos.offset >= _last_off)
                return 0;
            return static_cast<ssize_t>(_last_off);
        }

        size_t length = _locs.get(pos.local);
        if(rebind && length && _lastmem.sel() != _memcaps.start() + pos.local)
            _lastmem.rebind(_memcaps.start() + pos.local);
        return static_cast<ssize_t>(length);
    }
}

size_t RegularFile::serialize_length() {
    return ostreamsize<int, int, size_t, bool, size_t, Position, uint16_t, size_t>();
}

void RegularFile::delegate(VPE &) {
    // nothing to do, because we let the child fetch new mem caps
}

void RegularFile::serialize(Marshaller &m) {
    size_t mid = VPE::self().mounts()->get_mount_id(&*_fs);
    m << _fd << flags() << mid << _extended << _begin << _pos;
    m << _last_extent << _last_off;
}

RegularFile *RegularFile::unserialize(Unmarshaller &um) {
    int fd, flags;
    size_t mid;
    um >> fd >> flags >> mid;

    Reference<M3FS> fs(static_cast<M3FS*>(VPE::self().mounts()->get_mount(mid)));
    RegularFile *file = new RegularFile(fd, fs, flags);
    um >> file->_extended >> file->_begin >> file->_pos;
    um >> file->_last_extent >> file->_last_off;
    // we want to get new mem caps
    file->_pos.local = MAX_LOCS;
    return file;
}

}
