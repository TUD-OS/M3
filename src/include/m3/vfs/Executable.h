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

#include <m3/session/Pager.h>
#include <m3/stream/FStream.h>
#include <m3/vfs/RegularFile.h>

namespace m3 {

class Executable {
public:
    explicit Executable(int argc, const char **argv)
        // RWX because we may want to map it somewhere and need all permissions for that
        : _fs(argv[0], FILE_RWX), _argc(argc), _argv(argv) {
    }

    int argc() const {
        return _argc;
    }
    const char **argv() const {
        return _argv;
    }

    FStream &stream() {
        return _fs;
    }
    const Session &sess() const {
        const RegularFile *rfile = static_cast<const RegularFile*>(_fs.file());
        return *rfile->fs();
    }
    int fd() const {
        const RegularFile *rfile = static_cast<const RegularFile*>(_fs.file());
        return rfile->fd();
    }

private:
    FStream _fs;
    int _argc;
    const char **_argv;
};

}
