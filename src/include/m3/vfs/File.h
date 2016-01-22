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

#include <m3/Common.h>
#include <fs/internal.h>

namespace m3 {

class VFS;
class FStream;

/**
 * The base-class of all files. Can't be instantiated.
 */
class File {
    friend class FStream;

protected:
    explicit File(int perms = FILE_RW) : _flags(perms) {
    }

public:
    File(const File &) = delete;
    File &operator=(const File &) = delete;
    virtual ~File() {
    }

    /**
     * @return the flags (permissions, ...)
     */
    int flags() const {
        return _flags;
    }

    /**
     * @return if seeking is possible
     */
    virtual bool seekable() const = 0;

    /**
     * Retrieves information about this file
     *
     * @param info the struct to fill
     * @return 0 on success
     */
    virtual int stat(FileInfo &info) const = 0;

    /**
     * Changes the file-position to <offset>, using <whence>.
     *
     * @param offset the offset to use
     * @param whence the seek-type (SEEK_{SET,CUR,END}).
     * @return the new file-position
     */
    virtual off_t seek(off_t offset, int whence) = 0;

    /**
     * Reads at most <count> bytes into <buffer>.
     *
     * @param buffer the buffer to read into
     * @param count the number of bytes to read
     * @return the number of read bytes
     */
    virtual ssize_t read(void *buffer, size_t count) = 0;

    /**
     * Writes <count> bytes from <buffer> into the file.
     *
     * @param buffer the data to write
     * @param count the number of bytes to write
     * @return the number of written bytes
     */
    virtual ssize_t write(const void *buffer, size_t count) = 0;

private:
    virtual ssize_t fill(void *buffer, size_t size) = 0;
    virtual bool seek_to(off_t offset) = 0;

    int _flags;
};

}
