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
    virtual int stat(FileInfo &info) const override;

    virtual off_t seek(off_t, int) override {
        return 0;
    }

    virtual Buffer *create_buf(size_t size) override {
        return new File::Buffer(size);
    }

private:
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

    virtual char type() const override {
        return 'P';
    }
    virtual ssize_t read(void *buffer, size_t count) override {
        return _rd.read(buffer, count);
    }
    virtual ssize_t write(const void *, size_t) override {
        return 0;
    }

    virtual size_t serialize_length() override {
        return 0;
    }
    virtual void delegate(VPE &) override {
    }
    virtual void serialize(Marshaller &) override {
    }
    static PipeFileReader *unserialize(Unmarshaller &) {
        return nullptr;
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

    virtual char type() const override {
        return 'Q';
    }
    virtual ssize_t read(void *, size_t) override {
        return 0;
    }
    virtual ssize_t write(const void *buffer, size_t count) override {
        return _wr.write(buffer, count);
    }

    virtual size_t serialize_length() override {
        return 0;
    }
    virtual void delegate(VPE &) override {
    }
    virtual void serialize(Marshaller &) override {
    }
    static PipeFileWriter *unserialize(Unmarshaller &) {
        return nullptr;
    }

private:
    PipeWriter _wr;
};

}
