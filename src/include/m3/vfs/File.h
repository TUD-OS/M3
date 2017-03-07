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
    /**
     * The default buffer implementation
     */
    struct Buffer {
        /**
         * Creates a buffer with <_size> bytes.
         *
         * @param _size the number of bytes (0 = no buffer)
         */
        explicit Buffer(size_t _size)
            : buffer(_size ? new char[_size] : nullptr), size(_size), cur(), pos() {
            assert(Math::is_aligned(buffer, DTU_PKG_SIZE) && Math::is_aligned(size, DTU_PKG_SIZE));
        }
        virtual ~Buffer() {
            delete[] buffer;
        }

        /**
         * @return true if the buffer is empty
         */
        bool empty() {
            return cur == 0;
        }
        /**
         * Invalidates the buffer, i.e. makes it empty
         */
        void invalidate() {
            cur = 0;
        }

        /**
         * Puts the given character back into the buffer.
         *
         * @param off the current offset
         * @param c the character
         * @return true if successful
         */
        virtual bool putback(size_t off, char c);

        /**
         * Reads <amount> bytes from the buffer into <dst>.
         *
         * @param file the file backend
         * @param off the current offset
         * @param dst the destination buffer
         * @param amount the number of bytes to read
         * @return the number of read bytes (0 = EOF, <0 = error)
         */
        virtual ssize_t read(File *file, size_t off, void *dst, size_t amount);

        /**
         * Writes <amount> bytes from <src> into the buffer.
         *
         * @param file the file backend
         * @param off the current offset
         * @param src the data to write
         * @param amount the number of bytes to write
         * @return the number of written bytes (0 = EOF, <0 =  error)
         */
        virtual ssize_t write(File *file, size_t off, const void *src, size_t amount);

        /**
         * Seeks to given offset.
         *
         * @param off the current offset
         * @param whence the type of seek (SEEK_*)
         * @param offset the offset to seek to
         * @return >0 on seek, 0 on nothing done and <0 on error
         */
        virtual int seek(size_t off, int whence, size_t &offset);

        /**
         * Flushes the buffer.
         *
         * @param file the file backend
         * @return the number of bytes on success (0 = EOF, <0 =  error)
         */
        virtual ssize_t flush(File *file);

        char *buffer;
        size_t size;
        size_t cur;
        size_t pos;
    };

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
     * Creates a new buffer of given size
     *
     * @param size the size
     * @return the buffer
     */
    virtual Buffer *create_buf(size_t size) = 0;

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
    virtual size_t seek(size_t offset, int whence) = 0;

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

    /**
     * Determines the next chunk of data to read/write.
     *
     * @param memgate will be set to the selector of the memory gate to access that chunk
     * @param offset will be set to the offset in that memory gate
     * @param length will be set to length of the chunk in bytes
     * @return the error code, if any
     */
    virtual Errors::Code read_next(capsel_t */*memgate*/, size_t */*offset*/, size_t */*length*/) {
        return Errors::NOT_SUP;
    }
    virtual Errors::Code begin_write(capsel_t */*memgate*/, size_t */*offset*/, size_t */*length*/) {
        return Errors::NOT_SUP;
    }

    /**
     * Commits the write, that has been started with begin_write().
     *
     * @param length the number of bytes that have actually been written
     */
    virtual void commit_write(size_t /*length*/) {
    }

    /**
     * @return the unique character for serialization
     */
    virtual char type() const = 0;

    /**
     * Determines the number of bytes to serialize this object.
     *
     * @return the number of bytes
     */
    virtual size_t serialize_length() = 0;

    /**
     * Delegates all capabilities that are required for this file to the given VPE.
     *
     * @param vpe the VPE
     */
    virtual void delegate(VPE &vpe) = 0;

    /**
     * Serializes this object to the given marshaller.
     *
     * @param m the marshaller
     */
    virtual void serialize(Marshaller &m) = 0;

private:
    virtual bool seek_to(size_t offset) = 0;

    int _flags;
};

}
