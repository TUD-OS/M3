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

#include <base/Machine.h>

#include <m3/vfs/File.h>
#include <m3/VPE.h>

namespace m3 {

/**
 * The base-class for a file that reads/writes from/to a pipe. Can't be instantiated.
 */
class SerialFile : public File {
public:
    explicit SerialFile() : File(FILE_RW) {
    }

    virtual Errors::Code stat(FileInfo &) const override {
        // not supported
        return Errors::NOT_SUP;
    }
    virtual ssize_t seek(size_t, int) override {
        // not supported
        return Errors::NOT_SUP;
    }

    virtual ssize_t read(void *buffer, size_t count) override {
        return Machine::read(reinterpret_cast<char*>(buffer), count);
    }
    virtual ssize_t write(const void *buffer, size_t count) override {
        int res = Machine::write(reinterpret_cast<const char*>(buffer), count);
        return res < 0 ? res : static_cast<ssize_t>(count);
    }

    virtual File *clone() const override {
        return new SerialFile();
    }

    virtual char type() const override {
        return 'S';
    }
    virtual Errors::Code delegate(VPE &) override {
        // nothing to do
        return Errors::NONE;
    }
    virtual void serialize(Marshaller &) override {
        // nothing to do
    }
    static SerialFile *unserialize(Unmarshaller &) {
        return new SerialFile();
    }
};

}
