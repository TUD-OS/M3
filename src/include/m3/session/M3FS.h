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

#include <m3/session/Session.h>
#include <m3/com/RecvBuf.h>
#include <m3/com/SendGate.h>
#include <m3/vfs/FileSystem.h>

#include <fs/internal.h>

namespace m3 {

class M3FS : public Session, public FileSystem {
public:
    enum Operation {
        OPEN,
        STAT,
        FSTAT,
        SEEK,
        MKDIR,
        RMDIR,
        LINK,
        UNLINK,
        CLOSE,
        COUNT
    };

    enum Flags {
        BYTE_OFFSET = 1,
    };

    explicit M3FS(const String &service)
        : Session(service), FileSystem(), _gate(SendGate::bind(obtain(1).start())) {
    }
    explicit M3FS(capsel_t session, capsel_t gate)
        : Session(session), FileSystem(), _gate(SendGate::bind(gate)) {
    }

    const SendGate &gate() const {
        return _gate;
    }
    virtual char type() const override {
        return 'M';
    }

    virtual File *open(const char *path, int perms) override;
    virtual Errors::Code stat(const char *path, FileInfo &info) override;
    int fstat(int fd, FileInfo &info);
    int seek(int fd, off_t off, int whence, size_t &global, size_t &extoff, off_t &pos);
    virtual Errors::Code mkdir(const char *path, mode_t mode) override;
    virtual Errors::Code rmdir(const char *path) override;
    virtual Errors::Code link(const char *oldpath, const char *newpath) override;
    virtual Errors::Code unlink(const char *path) override;
    void close(int fd, size_t extent, size_t off);

    template<size_t N>
    bool get_locs(int fd, size_t offset, size_t count, size_t blocks, CapRngDesc &crd, LocList<N> &locs) {
        auto args = create_vmsg(fd, offset, count, blocks, 0);
        bool extended = false;
        GateIStream resp = obtain(count, crd, args);
        if(Errors::last == Errors::NO_ERROR)
            resp >> locs >> extended;
        return extended;
    }

private:
    SendGate _gate;
};

}
