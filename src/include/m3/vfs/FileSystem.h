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
#include <base/util/String.h>
#include <base/Errors.h>

#include <fs/internal.h>

namespace m3 {

class File;

/**
 * The base-class of all filesystems
 */
class FileSystem : public RefCounted {
public:
    explicit FileSystem() {
    }
    virtual ~FileSystem() {
    }

    /**
     * @return for serialization: the type of fs
     */
    virtual char type() const = 0;

    /**
     * Creates a File-instance from given path with given permissions.
     *
     * @param path the filepath
     * @param perms the permissions (FILE_*)
     * @return the File-instance or nullptr
     */
    virtual File *open(const char *path, int perms) = 0;

    /**
     * Retrieves the file information for the given path.
     *
     * @param path the path
     * @param info where to write to
     * @return the error, if any happened
     */
    virtual Errors::Code stat(const char *path, FileInfo &info) = 0;

    /**
     * Creates the given directory.
     *
     * @param path the directory path
     * @param mode the permissions to assign
     * @return Errors::NO_ERROR on success
     */
    virtual Errors::Code mkdir(const char *path, mode_t mode) = 0;

    /**
     * Removes the given directory. It needs to be empty.
     *
     * @param path the directory path
     * @return Errors::NO_ERROR on success
     */
    virtual Errors::Code rmdir(const char *path) = 0;

    /**
     * Creates a link at <newpath> to <oldpath>.
     *
     * @param oldpath the existing path
     * @param newpath tne link to create
     * @return Errors::NO_ERROR on success
     */
    virtual Errors::Code link(const char *oldpath, const char *newpath) = 0;

    /**
     * Removes the given file.
     *
     * @param path the path
     * @return Errors::NO_ERROR on success
     */
    virtual Errors::Code unlink(const char *path) = 0;
};

}
