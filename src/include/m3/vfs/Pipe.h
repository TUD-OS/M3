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

#include <m3/pipe/PipeReader.h>
#include <m3/pipe/PipeWriter.h>
#include <m3/vfs/File.h>

namespace m3 {

/**
 * The base-class for a file that reads/writes from/to a pipe. Can't be instantiated.
 */
class PipeFile : public File {
protected:
    explicit PipeFile() : File() {
    }

public:
    virtual bool seekable() const override {
        return false;
    }

    virtual int stat(FileInfo &info) const override {
        // TODO
        info.devno = 0;
        info.inode = 0;
        info.lastaccess = info.lastmod = 0;
        info.links = 1;
        info.mode = S_IFPIP | S_IRUSR | S_IWUSR;
        info.size = 0;
        return 0;
    }
    virtual off_t seek(off_t, int) override {
        return 0;
    }

private:
    virtual ssize_t fill(void *buffer, size_t size) override {
        memset(buffer, ' ', size);
        return size;
    }
    virtual bool seek_to(off_t) override {
        return false;
    }
};

/**
 * Implements File for reading from a pipe
 */
class PipeFileReader : public PipeFile {
public:
    explicit PipeFileReader(const Pipe &p) : PipeFile(), _rd(p) {
    }
    explicit PipeFileReader(capsel_t caps, size_t rep) : PipeFile(), _rd(caps, rep) {
    }

    virtual ssize_t read(void *buffer, size_t count) override {
        return _rd.read(buffer, count);
    }
    virtual ssize_t write(const void *, size_t) override {
        return 0;
    }

private:
    PipeReader _rd;
};

/**
 * Implements File for writing into a pipe
 */
class PipeFileWriter : public PipeFile {
public:
    explicit PipeFileWriter(const Pipe &p) : PipeFile(), _wr(p) {
    }
    explicit PipeFileWriter(capsel_t caps, size_t size) : PipeFile(), _wr(caps, size) {
    }

    virtual ssize_t read(void *, size_t) override {
        return 0;
    }
    virtual ssize_t write(const void *buffer, size_t count) override {
        return _wr.write(buffer, count);
    }

private:
    PipeWriter _wr;
};

}
