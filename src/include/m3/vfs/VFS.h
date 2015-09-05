/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/vfs/File.h>
#include <m3/vfs/FileSystem.h>
#include <m3/service/M3FS.h>
#include <m3/util/SList.h>
#include <m3/util/Reference.h>
#include <m3/DTU.h>

namespace m3 {

class RegularFile;

/**
 * A VPE-local virtual file system. It allows to mount filesystems at a given path and directs
 * filesystem operations like open, mkdir, ... to the corresponding filesystem.
 */
class VFS {
    friend class RegularFile;

    struct Init {
        Init();
        static Init obj;
    };

    class MountPoint : public SListItem {
    public:
        explicit MountPoint(const char *path, FileSystem *fs)
            : SListItem(), _path(path), _fs(fs) {
        }

        const String &path() const {
            return _path;
        }
        const Reference<FileSystem> &fs() const {
            return _fs;
        }

    private:
        String _path;
        Reference<FileSystem> _fs;
    };

public:
    /**
     * Mounts <fs> at given path
     *
     * @param path the path
     * @param fs the filesystem instance (has to be allocated on the heap; TODO keep that?)
     * @return Errors::NO_ERROR on success
     */
	static Errors::Code mount(const char *path, FileSystem *fs);

    /**
     * Unmounts the filesystem at <path>.
     *
     * @param path the path
     */
	static void unmount(const char *path);

    /**
     * Opens the file at <path> using the given permissions.
     *
     * @param path the path to the file to open
     * @param perms the permissions (FILE_*)
     * @return the File instance or nullptr if it failed
     */
    static File *open(const char *path, int perms);

    /**
     * Retrieves the file information for the given path.
     *
     * @param path the path
     * @param info where to write to
     * @return the error, if any happened
     */
    static Errors::Code stat(const char *path, FileInfo &info);

    /**
     * Creates the given directory. Expects that all path-components except the last already exists.
     *
     * @param path the path
     * @param mode the permissions to assign
     * @return the error, if any happened
     */
    static Errors::Code mkdir(const char *path, mode_t mode);

    /**
     * Removes the given directory. It needs to be empty.
     *
     * @param path the path
     * @return the error, if any happened
     */
    static Errors::Code rmdir(const char *path);

    /**
     * Creates a link at <newpath> to <oldpath>.
     *
     * @param oldpath the existing path
     * @param newpath tne link to create
     * @return the error, if any happened
     */
    static Errors::Code link(const char *oldpath, const char *newpath);

    /**
     * Removes the given path.
     *
     * @param path the path
     * @return the error, if any happened
     */
    static Errors::Code unlink(const char *path);

    /**
     * Determines the number of bytes for serializing the mounts.
     *
     * @return the number of bytes
     */
    static size_t serialize_length();

    /**
     * Serializes the current mounts into the given buffer
     *
     * @param buffer the buffer
     * @param size the capacity of the buffer
     * @return the space used
     */
    static size_t serialize(void *buffer, size_t size);

    /**
     * Delegates the capabilities necessary for given mounts to <vpe>.
     *
     * @param vpe the VPE to delegate the caps to
     * @param buffer the buffer with the serialized mounts
     * @param size the length of the data
     */
    static void delegate(VPE &vpe, const void *buffer, size_t size);

    /**
     * Unserializes the mounts from the buffer and mounts them
     *
     * @param buffer the buffer
     * @param size the length of the data
     */
    static void unserialize(const void *buffer, size_t size);

    /**
     * Prints the current mounts to <os>.
     *
     * @param os the stream to write to
     */
    static void print(OStream &os);

private:
    static size_t is_in_mount(const String &mount, const char *in);
    static Reference<FileSystem> resolve(const char *in, size_t *pos);

	static SList<MountPoint> _mounts;
};

}
