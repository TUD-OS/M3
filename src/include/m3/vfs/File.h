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
    explicit File() {
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
        }
        ~Buffer() {
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
         * @param c the character
         * @return true if successful
         */
        bool putback(char c);

        /**
         * Reads <amount> bytes from the buffer into <dst>.
         *
         * @param file the file backend
         * @param dst the destination buffer
         * @param amount the number of bytes to read
         * @return the number of read bytes (0 = EOF, <0 = error)
         */
        ssize_t read(File *file, void *dst, size_t amount);

        /**
         * Writes <amount> bytes from <src> into the buffer.
         *
         * @param file the file backend
         * @param src the data to write
         * @param amount the number of bytes to write
         * @return the number of written bytes (0 = EOF, <0 =  error)
         */
        ssize_t write(File *file, const void *src, size_t amount);

        /**
         * Flushes the buffer.
         *
         * @param file the file backend
         * @return the number of bytes on success (0 = EOF, <0 =  error)
         */
        ssize_t flush(File *file);

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
     * Retrieves information about this file
     *
     * @param info the struct to fill
     * @return the error code if any
     */
    virtual Errors::Code stat(FileInfo &info) const = 0;

    /**
     * Changes the file-position to <offset>, using <whence>.
     *
     * @param offset the offset to use
     * @param whence the seek-type (SEEK_{SET,CUR,END}).
     * @return the new file-position
     */
    virtual ssize_t seek(size_t offset, int whence) = 0;

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
     * Performs a flush of the so far written data
     */
    virtual void flush() {
    }

    /**
     * @return the unique character for serialization
     */
    virtual char type() const = 0;

    virtual File *clone() const = 0;

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
};

}
