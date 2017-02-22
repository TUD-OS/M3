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

#pragma once

#include <base/Common.h>
#include <base/stream/IStream.h>
#include <base/stream/OStream.h>

#include <m3/vfs/File.h>
#include <m3/vfs/FileTable.h>
#include <m3/VPE.h>

namespace m3 {

/**
 * FStream is an input- and output-stream for files. It uses m3::File as a backend and adds
 * buffering for the input and output.
 */
class FStream : public IStream, public OStream {
    static const uint FL_DEL_BUF    = 1;
    static const uint FL_DEL_FILE   = 2;

    static int get_perms(int perms) {
        // if we want to write, we need read-permission to handle unaligned writes
        if((perms & FILE_RW) == FILE_W)
            return perms | FILE_R;
        return perms;
    }

public:
    static const uint FL_LINE_BUF   = 4;

    /**
     * Binds this object to the given file descriptor and uses a buffer size of <bufsize>.
     *
     * @param fd the file descriptor
     * @param perms the permissions that determine which buffer to create (FILE_*)
     * @param bufsize the size of the buffer for input/output
     * @param flags the flags (FL_*)
     */
    explicit FStream(int fd, int perms = FILE_RW, size_t bufsize = 512, uint flags = 0);

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
     * Opens <filename> with given permissions and given buffer sizes.
     *
     * @param filename the file to open
     * @param rsize the size of the input-buffer (may be 0 if FILE_R is not set)
     * @param wsize the size of the output-buffer (may be 0 if FILE_W is not set)
     * @param perms the permissions (FILE_*)
     */
    explicit FStream(const char *filename, size_t rsize, size_t wsize, int perms = FILE_RW);

    virtual ~FStream();

    /**
     * @return the File instance
     */
    File *file() const {
        return VPE::self().fds()->get(_fd);
    }

    /**
     * Retrieves information about this file
     *
     * @param info the struct to fill
     * @return 0 on success
     */
    int stat(FileInfo &info) const {
        if(file())
            return file()->stat(info);
        return -1;
    }

    /**
     * Seeks to the given position.
     *
     * @param offset the offset to seek to (meaning depends on <whence>)
     * @param whence the seek type (SEEK_*)
     * @return the new position
     */
    size_t seek(size_t offset, int whence);

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
        if(_rbuf->putback(_fpos, c)) {
            _fpos--;
            return true;
        }
        return false;
    }
    virtual void write(char c) override {
        write(&c, 1);
    }

private:
    void set_error(ssize_t res);

    fd_t _fd;
    size_t _fpos;
    File::Buffer *_rbuf;
    File::Buffer *_wbuf;
    uint _flags;
};

}
