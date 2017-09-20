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
    static const size_t TMP_MEM_SIZE    = 1024;

public:
    explicit SerialFile() : File() {
    }

    virtual Errors::Code stat(FileInfo &) const override {
        // not supported
        return Errors::NOT_SUP;
    }
    virtual size_t seek(size_t, int) override {
        // not supported
        return 0;
    }

    virtual Buffer *create_buf(size_t size) override {
        return new File::Buffer(size);
    }

    virtual ssize_t read(void *buffer, size_t count) override {
        return Machine::read(reinterpret_cast<char*>(buffer), count);
    }
    virtual ssize_t write(const void *buffer, size_t count) override {
        int res = Machine::write(reinterpret_cast<const char*>(buffer), count);
        return res < 0 ? res : static_cast<ssize_t>(count);
    }

    virtual Errors::Code read_next(capsel_t *memgate, size_t *offset, size_t *length) override {
        char buffer[256];
        initTmpMem();

        ssize_t res = read(buffer, sizeof(buffer));
        if(res < 0)
            return Errors::last;

        tmp->write(buffer, static_cast<size_t>(res), 0);
        *length = static_cast<size_t>(res);
        *offset = 0;
        *memgate = tmp->sel();
        return Errors::NONE;
    }

    virtual Errors::Code begin_write(capsel_t *memgate, size_t *offset, size_t *length) override {
        initTmpMem();
        *length = TMP_MEM_SIZE;
        *offset = 0;
        *memgate = tmp->sel();
        return Errors::NONE;
    }

    virtual void commit_write(size_t length) override {
        char buffer[256];
        for(size_t amount, off = 0; off < length; off += amount) {
            amount = std::min(sizeof(buffer), length - off);
            tmp->read(buffer, amount, off);
            write(buffer, amount);
        }
    }

    virtual char type() const override {
        return 'S';
    }
    virtual size_t serialize_length() override {
        return 0;
    }
    virtual void delegate(VPE &) override {
        // nothing to do
    }
    virtual void serialize(Marshaller &) override {
        // nothing to do
    }
    static SerialFile *unserialize(Unmarshaller &) {
        return new SerialFile();
    }

private:
    virtual bool seek_to(size_t) override {
        return false;
    }

    void initTmpMem() {
        // TODO this does not work yet with the non-autonomous API of the hash accelerator. because
        // the caller will read have to read it back from the global memory, which requires him to
        // create a new MemGate and activate it. this does not work, since each capability can only
        // be activated on one EP.
        if(!tmp)
            tmp = new MemGate(MemGate::create_global(TMP_MEM_SIZE, MemGate::RW));
    }

    static MemGate *tmp;
};

}
