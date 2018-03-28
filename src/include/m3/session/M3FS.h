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

#include <base/util/Reference.h>

#include <m3/session/ClientSession.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/vfs/FileSystem.h>
#include <m3/vfs/GenericFile.h>

#include <fs/internal.h>

namespace m3 {

class M3FS : public ClientSession, public FileSystem {
public:
    enum Operation {
        FSTAT = GenericFile::STAT,
        SEEK = GenericFile::SEEK,
        READ = GenericFile::READ,
        WRITE = GenericFile::WRITE,
        STAT,
        MKDIR,
        RMDIR,
        LINK,
        UNLINK,
        COUNT
    };

    explicit M3FS(const String &service)
        : ClientSession(service, 0, VPE::self().alloc_sels(2)),
          FileSystem(),
          _gate(obtain_sgate()) {
    }
    explicit M3FS(capsel_t caps)
        : ClientSession(caps + 0),
          FileSystem(),
          _gate(SendGate::bind(caps + 1)) {
    }

    const SendGate &gate() const {
        return _gate;
    }
    virtual char type() const override {
        return 'M';
    }

    virtual File *open(const char *path, int perms) override;
    virtual Errors::Code stat(const char *path, FileInfo &info) override;
    virtual Errors::Code mkdir(const char *path, mode_t mode) override;
    virtual Errors::Code rmdir(const char *path) override;
    virtual Errors::Code link(const char *oldpath, const char *newpath) override;
    virtual Errors::Code unlink(const char *path) override;

    virtual Errors::Code delegate(VPE &vpe) override;
    virtual void serialize(Marshaller &m) override;
    static FileSystem *unserialize(Unmarshaller &um);

    // TODO wrong place. we should have a DataSpace session or something
    static size_t get_mem(ClientSession &sess, size_t *off, capsel_t *sel) {
        KIF::ExchangeArgs args;
        args.count = 1;
        args.vals[0] = *off;
        KIF::CapRngDesc crd = sess.obtain(1, &args);
        if(Errors::last == Errors::NONE) {
            *off = args.vals[0];
            *sel = crd.start();
            return args.vals[1];
        }
        return 0;
    }

private:
    SendGate obtain_sgate() {
        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, sel() + 1);
        obtain_for(VPE::self(), crd);
        return SendGate::bind(crd.start());
    }

    SendGate _gate;
};

template<>
struct OStreamSize<FileInfo> {
    static const size_t value = 9 * sizeof(xfer_t);
};

static inline Unmarshaller &operator>>(Unmarshaller &u, FileInfo &info) {
    u >> info.devno >> info.inode >> info.mode >> info.links >> info.size >> info.lastaccess
      >> info.lastmod >> info.extents >> info.firstblock;
    return u;
}

static inline GateIStream &operator>>(GateIStream &is, FileInfo &info) {
    is >> info.devno >> info.inode >> info.mode >> info.links >> info.size >> info.lastaccess
      >> info.lastmod >> info.extents >> info.firstblock;
    return is;
}

static inline Marshaller &operator<<(Marshaller &m, const FileInfo &info) {
    m << info.devno << info.inode << info.mode << info.links << info.size << info.lastaccess
      << info.lastmod << info.extents << info.firstblock;
    return m;
}

}
