/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#pragma once

#include <m3/Common.h>
#include <m3/stream/IStream.h>
#include <m3/stream/OStream.h>
#include <m3/vfs/File.h>
#include <m3/vfs/VFS.h>

namespace m3 {

/**
 * FStream is an input- and output-stream for files. It uses m3::File as a backend and adds
 * buffering for the input and output.
 */
class FStream : public IStream, public OStream {
    struct Buffer {
        explicit Buffer(char *_data, size_t _size) : data(_data), size(_size), cur(), pos() {
            assert(Math::is_aligned(data, DTU_PKG_SIZE) && Math::is_aligned(size, DTU_PKG_SIZE));
        }
        ~Buffer() {
            if(data)
                delete[] data;
        }

        char *data;
        size_t size;
        size_t cur;
        off_t pos;
    };

    static int get_perms(int perms) {
        // if we want to write, we need read-permission to handle unaligned writes
        if((perms & FILE_RW) == FILE_W)
            return perms | FILE_R;
        return perms;
    }

public:
    /**
     * Opens <filename> with given permissions and a buffer size of <bufsize>. Which buffer is
     * created depends on <perms>.
     *
     * @param filename the file to open
     * @param perms the permissions (FILE_*)
     * @param bufsize the size of the buffer for input/output
     */
    explicit FStream(const char *filename, int perms = FILE_RW, size_t bufsize = 512);

    /**
     * Opens <filename> with given permissions and given buffers.
     *
     * @param filename the file to open
     * @param rbuf the input-buffer (may be nullptr if FILE_R is not set)
     * @param rsize the size of the input-buffer (may be 0 if FILE_R is not set)
     * @param wbuf the output-buffer (may be nullptr if FILE_W is not set)
     * @param wsize the size of the output-buffer (may be 0 if FILE_W is not set)
     * @param perms the permissions (FILE_*)
     */
    explicit FStream(const char *filename, char *rbuf, size_t rsize,
            char *wbuf, size_t wsize, int perms = FILE_RW);

    virtual ~FStream();

    /**
     * @return the File instance
     */
    const File &file() const {
        return *_file;
    }

    /**
     * Retrieves information about this file
     *
     * @param info the struct to fill
     * @return 0 on success
     */
    int stat(FileInfo &info) const {
        return _file->stat(info);
    }

    /**
     * Seeks to the given position.
     *
     * @param offset the offset to seek to (meaning depends on <whence>)
     * @param whence the seek type (SEEK_*)
     * @return the new position
     */
    off_t seek(off_t offset, int whence);

    /**
     * Reads <count> bytes into <dst>. If both is aligned by DTU_PKG_SIZE and the buffer is
     * empty, the buffer is not used but it the File instance is used directly.
     *
     * @param dst the destination to read into
     * @param count the number of bytes to read
     * @return the number of read bytes
     */
    size_t read(void *dst, size_t count);

    /**
     * Writes <count> bytes from <src> into the file. If both is aligned by DTU_PKG_SIZE and
     * the buffer is empty, the buffer is not used but it the File instance is used directly.

     * @param src the data to write
     * @param count the number of bytes to write
     * @return the number of written bytes
     */
    size_t write(const void *src, size_t count);

    /**
     * Flushes the internal write buffer
     */
    void flush();

    virtual char read() override {
        char c;
        read(&c, 1);
        return c;
    }
    virtual bool putback(char c) override {
        if(!_rbuf.cur || _fpos <= _rbuf.pos || _fpos > (off_t)(_rbuf.pos + _rbuf.cur))
            return false;
        _rbuf.data[--_fpos - _rbuf.pos] = c;
        return true;
    }
    virtual void write(char c) override {
        write(&c, 1);
    }

private:
    off_t do_seek(off_t offset, int whence);
    void set_error(ssize_t res);

    File *_file;
    off_t _fpos;
    Buffer _rbuf;
    Buffer _wbuf;
    bool _del;
};

}
