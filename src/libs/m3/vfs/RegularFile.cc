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

#include <m3/vfs/RegularFile.h>
#include <m3/vfs/VFS.h>
#include <m3/service/M3FS.h>
#include <m3/Log.h>

namespace m3 {

RegularFile::RegularFile(int fd, Reference<M3FS> fs, int perms)
    : File(perms), _fd(fd), _extended(), _begin(), _length(), _pos(),
      /* pass an arbitrary selector first */
      _memcaps(), _locs(), _lastmem(MemGate::bind(0)), _last_extent(0), _last_off(0),
      _fs(fs) {
    if(flags() & FILE_APPEND)
        seek(0, SEEK_END);
}

RegularFile::~RegularFile() {
    // the fs-service will revoke it. so don't try to "unactivate" it afterwards
    _lastmem.rebind(Cap::INVALID);
    if(_fs.valid())
        _fs->close(_fd, _last_extent, _last_off);
    _memcaps.free();
}

int RegularFile::stat(FileInfo &info) const {
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

off_t RegularFile::seek(off_t off, int whence) {
    assert((off & (DTU_PKG_SIZE - 1)) == 0);
    size_t global, extoff;
    off_t pos;
    // optimize that special case
    if(whence == SEEK_SET && off == 0) {
        global = 0;
        extoff = 0;
        pos = 0;
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
    return pos;
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

ssize_t RegularFile::do_read(void *buffer, size_t count, Position &pos) const {
    assert(Math::is_aligned(buffer, DTU_PKG_SIZE) && Math::is_aligned(count, DTU_PKG_SIZE));
    if(~flags() & FILE_R)
        return Errors::NO_PERM;

    char *buf = reinterpret_cast<char*>(buffer);
    while(count > 0) {
        // figure out where that part of the file is in memory, based on our location db
        ssize_t extlen = get_location(pos, false);
        if(extlen < 0)
            return extlen;
        if(extlen == 0)
            break;

        // determine next off and idx
        size_t memoff = pos.offset;
        size_t amount = get_amount(extlen, count, pos);

        // read from global memory
        // we need to round up here because the filesize might not be a multiple of DTU_PKG_SIZE
        // in which case the last extent-size is not aligned
        _lastmem.read_sync(buf, Math::round_up(amount, DTU_PKG_SIZE), memoff);
        buf += amount;
        count -= amount;
    }
    return buf - reinterpret_cast<char*>(buffer);
}

ssize_t RegularFile::do_write(const void *buffer, size_t count, Position &pos) const {
    assert(Math::is_aligned(buffer, DTU_PKG_SIZE) && Math::is_aligned(count, DTU_PKG_SIZE));
    if(~flags() & FILE_W)
        return Errors::NO_PERM;

    const char *buf = reinterpret_cast<const char*>(buffer);
    while(count > 0) {
        // figure out where that part of the file is in memory, based on our location db
        ssize_t extlen = get_location(pos, true);
        if(extlen < 0)
            return extlen;
        if(extlen == 0)
            break;

        // determine next off and idx
        uint16_t lastglobal = pos.global;
        size_t memoff = pos.offset;
        size_t amount = get_amount(extlen, count, pos);

        // remember the max. position we wrote to
        if(lastglobal >= _last_extent) {
            if(lastglobal > _last_extent || memoff + amount > _last_off)
                _last_off = memoff + amount;
            _last_extent = lastglobal;
        }

        // write to global memory
        _lastmem.write_sync(buf, amount, memoff);
        buf += amount;
        count -= amount;
    }
    return buf - reinterpret_cast<const char*>(buffer);
}

ssize_t RegularFile::get_location(Position &pos, bool writing) const {
    if(!pos.valid() || (writing && _locs.get(pos.local) == 0)) {
        // the fs-service will revoke our memory-caps. thus, we have to tell that to our gate
        // so that it passes Cap::INVALID as the old cap on the next ep-switch.
        _lastmem.rebind(Cap::INVALID);
        _memcaps.free();
        _locs.clear();

        // move forward
        _begin += _length;
        _length = 0;

        // get new locations
        pos.local = 0;
        // TODO it would be better to increment the number of blocks we create, like start with
        // 4, then 8, then 16, up to a certain limit.
        _extended |= const_cast<Reference<M3FS>&>(_fs)->get_locs(_fd, pos.global, MAX_LOCS,
            writing ? WRITE_INC_BLOCKS : 0, _memcaps, _locs);
        if(Errors::last != Errors::NO_ERROR || _locs.count() == 0)
            return Errors::last;

        // determine new length
        for(size_t i = 0; i < _locs.count(); ++i)
            _length += _locs.get(i);

        // when seeking to the end, we might be already at the end of a extent. if that happened,
        // go to the beginning of the next one
        size_t length = _locs.get(0);
        if(pos.offset == length)
            pos.next_extent();
        _lastmem.rebind(_memcaps.start() + pos.local);
        return _locs.get(pos.local);
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
            return _last_off;
        }

        size_t length = _locs.get(pos.local);
        if(length && _lastmem.sel() != _memcaps.start() + pos.local)
            _lastmem.rebind(_memcaps.start() + pos.local);
        return length;
    }
}

ssize_t RegularFile::fill(void *buffer, size_t size) {
    // don't change our internal position
    Position pos = _pos;
    ssize_t res = do_read(buffer, size, pos);
    if(res == 0) {
        memset(buffer, 0, size);
        return size;
    }
    return res;
}

bool RegularFile::seek_to(off_t newpos) {
    // is it already in our local data?
    if(_pos.valid() && newpos >= _begin && newpos < _begin + _length) {
        off_t begin = _begin;
        for(size_t i = 0; i < MAX_LOCS; ++i) {
            size_t len = _locs.get(i);
            if(!len || (newpos >= begin && newpos < static_cast<off_t>(begin + len))) {
                _pos.global += i - _pos.local;
                _pos.local = i;
                // this has to be aligned. read() will consider this
                _pos.offset = (newpos - begin) & ~(DTU_PKG_SIZE - 1);
                adjust_written_part();
                return true;
            }
            begin += len;
        }
    }
    return false;
}

}
