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

#include <m3/Common.h>
#include <m3/vfs/File.h>
#include <m3/vfs/VFS.h>

namespace m3 {

/**
 * Convenience class that opens a file, stores it and deletes it on destruction.
 */
class FileRef {
public:
    /**
     * Opens the file given by <path>.
     *
     * @param path the path to open
     * @param perms the permissions (FILE_*)
     */
    explicit FileRef(const char *path, int perms) : _file(VFS::open(path, perms)) {
    }
    FileRef(FileRef &&f) : _file(f._file) {
        f._file = nullptr;
    }
    FileRef(const FileRef&) = delete;
    FileRef &operator=(const FileRef&) = delete;
    ~FileRef() {
        delete _file;
    }

    File *operator->() {
        return _file;
    }
    const File *operator->() const {
        return _file;
    }
    File &operator*() {
        return *_file;
    }
    const File &operator*() const {
        return *_file;
    }

private:
    File *_file;
};

}
