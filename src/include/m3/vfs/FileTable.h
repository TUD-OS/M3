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
#include <base/Errors.h>
#include <base/DTU.h>

#include <assert.h>

namespace m3 {

class File;
class GenericFile;
class VPE;

/**
 * The file descriptor table.
 *
 * The file table itself does not create or delete files. Instead, it only works with
 * pointers. The creation and deletion is done in VFS. The rational is, that VFS is used to
 * work with files, while FileTable is used to prepare the files for created VPEs. Thus, one
 * can simply add a file or remove a file from VPE::self() to a different VPE by passing a pointer
 * around. If the file table of a child VPE is completely setup, it is serialized and transferred
 * to the child VPE.
 */
class FileTable {
    friend class GenericFile;

    struct FileEp {
        GenericFile *file;
        epid_t epid;
    };

public:
    static const fd_t MAX_EPS       = EP_COUNT / 4;
    static const fd_t MAX_FDS       = 64;
    static const fd_t INVALID       = MAX_FDS;

    /**
     * Constructor
     */
    explicit FileTable()
        : _file_ep_count(),
          _file_ep_victim(),
          _file_eps(),
          _fds() {
    }

    explicit FileTable(const FileTable &f) {
        for(fd_t i = 0; i < MAX_FDS; ++i)
            _fds[i] = f._fds[i];
    }
    FileTable &operator=(const FileTable &f) {
        if(&f != this) {
            for(fd_t i = 0; i < MAX_FDS; ++i)
                _fds[i] = f._fds[i];
        }
        return *this;
    }

    /**
     * Allocates a new file descriptor for given file.
     *
     * @param file the file
     * @return the file descriptor or MAX_FDS if all fds are in use
     */
    fd_t alloc(File *file);

    /**
     * Free's the given file descriptor
     *
     * @param fd the file descriptor
     */
    File *free(fd_t fd);

    /**
     * @param fd the file descriptor
     * @return true if the given file descriptor exists
     */
    bool exists(fd_t fd) const {
        return _fds[fd] != nullptr;
    }

    /**
     * @param fd the file descriptor
     * @return the file for given fd
     */
    File *get(fd_t fd) const {
        return _fds[fd];
    }

    /**
     * Sets <fd> to <file>.
     *
     * @param fd the file descriptor
     * @param file the file
     */
    void set(fd_t fd, File *file) {
        assert(file != nullptr);
        _fds[fd] = file;
    }

    /**
     * Delegates all files to <vpe>.
     *
     * @param vpe the VPE to delegate the files to
     * @return the error, if any
     */
    Errors::Code delegate(VPE &vpe) const;

    /**
     * Serializes the current files into the given buffer
     *
     * @param buffer the buffer
     * @param size the capacity of the buffer
     * @return the space used
     */
    size_t serialize(void *buffer, size_t size) const;

    /**
     * Unserializes the given buffer into a new FileTable object.
     *
     * @param buffer the buffer
     * @param size the size of the buffer
     * @return the FileTable object
     */
    static FileTable *unserialize(const void *buffer, size_t size);

private:
    epid_t request_ep(GenericFile *file);

    size_t _file_ep_count;
    size_t _file_ep_victim;
    FileEp _file_eps[MAX_EPS];
    File *_fds[MAX_FDS];
};

}
