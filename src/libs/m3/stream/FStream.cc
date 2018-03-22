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
#include <m3/vfs/VFS.h>

namespace m3 {

FStream::FStream(int fd, int perms, size_t bufsize, uint flags)
    : IStream(),
      OStream(),
      _fd(fd),
      _rbuf(new File::Buffer((perms & FILE_R) ? bufsize : 0)),
      _wbuf(new File::Buffer((perms & FILE_W) ? bufsize : 0)),
      _flags(FL_DEL_BUF | flags) {
}

FStream::FStream(const char *filename, int perms, size_t bufsize)
    : FStream(filename, bufsize, bufsize, perms) {
    if(_fd == FileTable::INVALID)
        _state |= FL_ERROR;
}

FStream::FStream(const char *filename, size_t rsize, size_t wsize, int perms)
    : IStream(),
      OStream(),
      _fd(VFS::open(filename, get_perms(perms))),
      _rbuf(_fd != FileTable::INVALID ? new File::Buffer((perms & FILE_R) ? rsize : 0) : nullptr),
      _wbuf(_fd != FileTable::INVALID ? new File::Buffer((perms & FILE_W) ? wsize : 0) : nullptr),
      _flags(FL_DEL_BUF | FL_DEL_FILE) {
    if(_fd == FileTable::INVALID)
        _state |= FL_ERROR;
}

FStream::~FStream() {
    flush();
    if(!(_flags & FL_DEL_BUF)) {
        if(_rbuf)
            _rbuf->buffer = nullptr;
        if(_wbuf)
            _wbuf->buffer = nullptr;
    }
    if((_flags & FL_DEL_FILE) && _fd != FileTable::INVALID)
        VFS::close(_fd);
    delete _rbuf;
    delete _wbuf;
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

    // use the unbuffered read, if the buffer is smaller
    if(_rbuf->empty() && count > _rbuf->size) {
        ssize_t res = file()->read(dst, count);
        if(res <= 0)
            set_error(res);
        return res < 0 ? 0 : static_cast<size_t>(res);
    }

    if(!_rbuf->buffer) {
        _state |= FL_ERROR;
        return 0;
    }

    size_t total = 0;
    char *buf = reinterpret_cast<char*>(dst);
    while(count > 0) {
        ssize_t res = _rbuf->read(file(), buf + total, count);
        if(res <= 0) {
            set_error(res);
            return total;
        }
        total += static_cast<size_t>(res);
        count -= static_cast<size_t>(res);
    }

    return total;
}

void FStream::flush() {
    if(_wbuf && file()) {
        set_error(_wbuf->flush(file()));
        file()->flush();
    }
}

size_t FStream::seek(size_t offset, int whence) {
    if(error())
        return 0;

    if(whence != M3FS_SEEK_CUR || offset != 0) {
        // TODO for simplicity, we always flush the write-buffer if we're changing the position
        flush();
    }

    // on relative seeks, take our position within the buffer into account
    if(whence == M3FS_SEEK_CUR)
        offset -= _rbuf->cur - _rbuf->pos;

    ssize_t res = file()->seek(offset, whence);
    if(res < 0) {
        _state |= FL_ERROR;
        return 0;
    }
    _rbuf->invalidate();
    return static_cast<size_t>(res);
}

size_t FStream::write(const void *src, size_t count) {
    if(bad())
        return 0;

    // use the unbuffered write, if the buffer is smaller
    if(_wbuf->empty() && count > _wbuf->size) {
        ssize_t res = file()->write(src, count);
        set_error(res);
        return res < 0 ? 0 : static_cast<size_t>(res);
    }

    if(!_wbuf->buffer) {
        _state |= FL_ERROR;
        return 0;
    }

    const char *buf = reinterpret_cast<const char*>(src);
    size_t total = 0;
    while(count > 0) {
        ssize_t res = _wbuf->write(file(), buf + total, count);
        if(res <= 0) {
            set_error(res);
            return 0;
        }

        total += static_cast<size_t>(res);
        count -= static_cast<size_t>(res);

        if(((_flags & FL_LINE_BUF) && buf[total - 1] == '\n'))
            flush();
        else if(count)
            set_error(_wbuf->flush(file()));
    }

    return total;
}

}
