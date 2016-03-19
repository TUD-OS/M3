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
#include <m3/stream/FStream.h>

namespace m3 {

FStream::FStream(int fd, size_t bufsize)
    : IStream(), OStream(), _fd(fd), _fpos(),
      _rbuf(new char[bufsize], bufsize), _wbuf(new char[bufsize], bufsize), _flags(FL_DEL_BUF) {
}

FStream::FStream(const char *filename, int perms, size_t bufsize)
    : IStream(), OStream(), _fd(VFS::open(filename, get_perms(perms))), _fpos(),
      _rbuf((perms & FILE_R) ? new char[bufsize] : nullptr, bufsize),
      _wbuf((perms & FILE_W) ? new char[bufsize] : nullptr, bufsize),
      _flags(FL_DEL_BUF | FL_DEL_FILE) {
    if(_fd == FileTable::INVALID)
        _state |= FL_ERROR;
}

FStream::FStream(const char *filename, char *rbuf, size_t rsize,
        char *wbuf, size_t wsize, int perms)
    : IStream(), OStream(), _fd(VFS::open(filename, get_perms(perms))), _fpos(),
      _rbuf(rbuf, rsize), _wbuf(wbuf, wsize),
      _flags(FL_DEL_FILE) {
    if(_fd == FileTable::INVALID)
        _state |= FL_ERROR;
}

FStream::~FStream() {
    flush();
    if(!(_flags & FL_DEL_BUF)) {
        _rbuf.data = nullptr;
        _wbuf.data = nullptr;
    }
    if((_flags & FL_DEL_FILE) && _fd != FileTable::INVALID)
        VFS::close(_fd);
}

void FStream::set_error(ssize_t res) {
    if(res < 0)
        _state |= FL_ERROR;
    else if(res == 0)
        _state |= FL_EOF;
}

size_t FStream::read(void *dst, size_t count) {
    if(bad())
        return 0;

    // ensure that our write-buffer is empty
    // TODO maybe it's better to have just one buffer for both and track dirty regions?
    flush();

    // simply use the unbuffered read, if the buffer is empty and all is aligned
    if(!_rbuf.cur && Math::is_aligned(_fpos, DTU_PKG_SIZE) &&
                     Math::is_aligned(dst, DTU_PKG_SIZE) &&
                     Math::is_aligned(count, DTU_PKG_SIZE)) {
        ssize_t res = file()->read(dst, count);
        if(res > 0)
            _fpos += res;
        else
            set_error(res);
        return res < 0 ? 0 : res;
    }

    if(!_rbuf.data) {
        _state |= FL_ERROR;
        return 0;
    }

    size_t total = 0;
    char *buf = reinterpret_cast<char*>(dst);
    // is there some data already in our buffer?
    if(_rbuf.cur && _fpos >= _rbuf.pos && _fpos < static_cast<off_t>(_rbuf.pos + _rbuf.cur)) {
        size_t amount = std::min<size_t>(count, _rbuf.pos + _rbuf.cur - _fpos);
        memcpy(buf, _rbuf.data + (_fpos - _rbuf.pos), amount);
        count -= amount;
        total += amount;
    }

    size_t posoff = (_fpos + total) & (DTU_PKG_SIZE - 1);
    while(count > 0) {
        _rbuf.pos = _fpos + total - posoff;
        // we can assume here that we are always at the position (_idx, _off), because our
        // read-buffer is empty, which means that we've used everything that we read via _file->read
        // last time.
        ssize_t res = file()->read(_rbuf.data, _rbuf.size);
        if(res <= 0) {
            set_error(res);
            _rbuf.cur = 0;
            return total;
        }
        _rbuf.cur = res;
        size_t amount = std::min(std::min(static_cast<size_t>(res), _rbuf.size - posoff), count);
        memcpy(buf + total, _rbuf.data + posoff, amount);
        total += amount;
        count -= amount;
        posoff = 0;
    }

    _fpos += total;
    return total;
}

void FStream::flush() {
    if(_wbuf.cur > 0) {
        size_t posoff = _wbuf.cur & (DTU_PKG_SIZE - 1);
        // first, write the aligned part
        if(_wbuf.cur - posoff > 0)
            set_error(file()->write(_wbuf.data, _wbuf.cur - posoff));

        // if there is anything left, read that first and write it back
        if(posoff != 0) {
            alignas(DTU_PKG_SIZE) uint8_t tmpbuf[DTU_PKG_SIZE];
            set_error(file()->fill(tmpbuf, DTU_PKG_SIZE));
            memcpy(tmpbuf, _wbuf.data + _wbuf.cur - posoff, posoff);
            set_error(file()->write(tmpbuf, DTU_PKG_SIZE));
        }
        _wbuf.cur = 0;
    }
}

off_t FStream::seek(off_t offset, int whence) {
    if(!file()->seekable()) {
        _state |= FL_ERROR;
        return 0;
    }

    if(whence != SEEK_CUR || offset != 0) {
        // TODO for simplicity, we always flush the write-buffer if we're changing the position
        flush();
    }
    return do_seek(offset, whence);
}

off_t FStream::do_seek(off_t offset, int whence) {
    if(whence != SEEK_END) {
        // if we seek within our read-buffer, it's enough to set the position
        off_t newpos = whence == SEEK_CUR ? _fpos + offset : offset;
        if(_rbuf.cur) {
            if(newpos >= _rbuf.pos && newpos <= static_cast<off_t>(_rbuf.pos + _rbuf.cur)) {
                _fpos = newpos;
                return _fpos;
            }
        }

        if(file()->seek_to(newpos)) {
            _fpos = newpos;
            // the buffer is invalid now
            _rbuf.cur = 0;
            return _fpos;
        }
    }

    // File::seek assumes that it is aligned. _fpos needs to reflect the actual position, of course
    size_t posoff = offset & (DTU_PKG_SIZE - 1);
    _fpos = file()->seek(offset - posoff, whence) + posoff;
    _rbuf.cur = 0;
    return _fpos;
}

size_t FStream::write(const void *src, size_t count) {
    if(bad())
        return 0;

    // simply use the unbuffered write, if the buffer is empty and all is aligned
    if(!_wbuf.cur && Math::is_aligned(_fpos, DTU_PKG_SIZE) &&
                     Math::is_aligned(src, DTU_PKG_SIZE) &&
                     Math::is_aligned(count, DTU_PKG_SIZE)) {
        ssize_t res = file()->write(src, count);
        set_error(res);
        return res < 0 ? 0 : res;
    }

    if(!_wbuf.data) {
        _state |= FL_ERROR;
        return 0;
    }

    const char *buf = reinterpret_cast<const char*>(src);
    size_t total = 0;
    size_t posoff = _fpos & (DTU_PKG_SIZE - 1);
    while(count > 0) {
        if(_wbuf.cur == 0) {
            _wbuf.pos = _fpos + total - posoff;
            _wbuf.cur = posoff;
            if(_wbuf.cur > 0) {
                ssize_t res = file()->fill(_wbuf.data, DTU_PKG_SIZE);
                if(res <= 0) {
                    set_error(res);
                    return res;
                }
            }
        }

        size_t amount = std::min(_wbuf.size - _wbuf.cur, count);
        memcpy(_wbuf.data + _wbuf.cur, buf + total, amount);
        _wbuf.cur += amount;
        total += amount;
        count -= amount;
        posoff = 0;

        if(count)
            flush();
    }

    _fpos += total;
    return total;
}

}
