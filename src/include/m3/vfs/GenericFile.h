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

#include <m3/com/SendGate.h>
#include <m3/com/MemGate.h>
#include <m3/session/Session.h>
#include <m3/vfs/File.h>
#include <m3/VPE.h>

namespace m3 {

class GenericFile : public File {
    friend class FileTable;

public:
    enum Operation {
        STAT,
        SEEK,
        READ,
        WRITE,
        COUNT,
    };

    explicit GenericFile(capsel_t caps)
        : File(),
          _sess(caps + 0, 0), _sg(SendGate::bind(caps + 1)), _mg(MemGate::bind(ObjCap::INVALID)),
          _goff(), _off(), _pos(), _len(), _writing() {
    }
    virtual ~GenericFile();

    SendGate &sgate() {
        return _sg;
    }
    Session &sess() {
        return _sess;
    }
    const Session &sess() const {
        return _sess;
    }

    virtual Errors::Code stat(FileInfo &info) const override;

    virtual ssize_t seek(size_t offset, int whence) override;

    virtual ssize_t read(void *buffer, size_t count) override;
    virtual ssize_t write(const void *buffer, size_t count) override;

    virtual void flush() override {
        submit(false);
    }

    virtual char type() const override {
        return 'F';
    }

    virtual File *clone() const override {
        KIF::CapRngDesc crd = _sess.obtain(2);
        return new GenericFile(crd.start());
    }

    virtual size_t serialize_length() override {
        return ostreamsize<capsel_t>();
    }

    virtual void delegate(VPE &vpe) override {
        // TODO what if it fails?
        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, _sess.sel(), 2);
        _sess.obtain_for(vpe, crd);
    }

    virtual void serialize(Marshaller &m) override {
        m << _sess.sel();
    }

    static File *unserialize(Unmarshaller &um) {
        capsel_t caps;
        um >> caps;
        return new GenericFile(caps);
    }

private:
    void evict();
    Errors::Code submit(bool force);
    Errors::Code delegate_ep();

    mutable Session _sess;
    mutable SendGate _sg;
    MemGate _mg;
    size_t _goff;
    size_t _off;
    size_t _pos;
    size_t _len;
    bool _writing;
};

}
