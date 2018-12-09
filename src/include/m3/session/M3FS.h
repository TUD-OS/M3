/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

class GenericFile;

class M3FS : public ClientSession, public FileSystem {
public:
    friend class GenericFile;

    enum Operation {
        FSTAT = GenericFile::STAT,
        SEEK = GenericFile::SEEK,
        NEXT_IN = GenericFile::NEXT_IN,
        NEXT_OUT = GenericFile::NEXT_OUT,
        COMMIT = GenericFile::COMMIT,
        STAT,
        MKDIR,
        RMDIR,
        LINK,
        UNLINK,
        OPEN_PRIV,
        CLOSE_PRIV,
        COUNT
    };

    explicit M3FS(const String &service)
        : ClientSession(service, 0, VPE::self().alloc_sels(2)),
          FileSystem(),
          _gate(obtain_sgate()),
          _eps(),
          _eps_count(),
          _eps_used() {
    }
    explicit M3FS(capsel_t caps)
        : ClientSession(caps + 0),
          FileSystem(),
          _gate(SendGate::bind(caps + 1)),
          _eps(),
          _eps_count(),
          _eps_used() {
    }

    const SendGate &gate() const {
        return _gate;
    }
    virtual char type() const override {
        return 'M';
    }

    virtual Errors::Code delegate_eps(capsel_t first, uint count) override {
        Errors::Code res = ClientSession::delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, first, count));
        if(res != Errors::NONE)
            return res;
        _eps = first;
        _eps_count = count;
        _eps_used = 0;
        return Errors::NONE;
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
    capsel_t alloc_ep() {
        for(uint i = 0; i < _eps_count; ++i) {
            if((_eps_used & (1u << i)) == 0) {
                _eps_used |= 1u << i;
                return _eps + i;
            }
        }
        return ObjCap::INVALID;
    }
    void free_ep(capsel_t ep) {
        _eps_used &= ~(1u << (ep - _eps));
    }
    SendGate obtain_sgate() {
        KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, sel() + 1);
        obtain_for(VPE::self(), crd);
        return SendGate::bind(crd.start());
    }

    SendGate _gate;
    capsel_t _eps;
    uint _eps_count;
    uint _eps_used;
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
