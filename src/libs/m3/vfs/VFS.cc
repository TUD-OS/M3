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

#include <base/com/Marshalling.h>
#include <base/stream/Serial.h>

#include <m3/vfs/File.h>
#include <m3/vfs/FileTable.h>
#include <m3/vfs/MountSpace.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>

namespace m3 {

// clean them up after the standard streams have been destructed
VFS::Cleanup VFS::_cleanup INIT_PRIORITY(103);

VFS::Cleanup::~Cleanup() {
    for(fd_t i = 0; i < FileTable::MAX_FDS; ++i)
        delete VPE::self().fds()->free(i);
}

MountSpace *VFS::ms() {
    return VPE::self().mountspace();
}

Errors::Code VFS::mount(const char *path, FileSystem *fs) {
    return ms()->add(new MountSpace::MountPoint(path, fs));
}

void VFS::unmount(const char *path) {
    MountSpace::MountPoint *mp = ms()->remove(path);
    if(mp)
        delete mp;
}

fd_t VFS::open(const char *path, int perms) {
    size_t pos;
    Reference<FileSystem> fs = ms()->resolve(path, &pos);
    if(!fs.valid()) {
        Errors::last = Errors::NO_SUCH_FILE;
        return FileTable::INVALID;
    }
    File *file = fs->open(path + pos, perms);
    if(file) {
        fd_t fd = VPE::self().fds()->alloc(file);
        if(fd == FileTable::INVALID) {
            delete file;
            Errors::last = Errors::NO_SPACE;
        }
        return fd;
    }
    return FileTable::INVALID;
}

void VFS::close(fd_t fd) {
    File *file = VPE::self().fds()->free(fd);
    delete file;
}

Errors::Code VFS::stat(const char *path, FileInfo &info) {
    size_t pos;
    Reference<FileSystem> fs = ms()->resolve(path, &pos);
    if(!fs.valid())
        return Errors::last = Errors::NO_SUCH_FILE;
    return fs->stat(path + pos, info);
}

Errors::Code VFS::mkdir(const char *path, mode_t mode) {
    size_t pos;
    Reference<FileSystem> fs = ms()->resolve(path, &pos);
    if(!fs.valid())
        return Errors::last = Errors::NO_SUCH_FILE;
    return fs->mkdir(path + pos, mode);
}

Errors::Code VFS::rmdir(const char *path) {
    size_t pos;
    Reference<FileSystem> fs = ms()->resolve(path, &pos);
    if(!fs.valid())
        return Errors::last = Errors::NO_SUCH_FILE;
    return fs->rmdir(path + pos);
}

Errors::Code VFS::link(const char *oldpath, const char *newpath) {
    size_t pos1, pos2;
    Reference<FileSystem> fs1 = ms()->resolve(oldpath, &pos1);
    if(!fs1.valid())
        return Errors::last = Errors::NO_SUCH_FILE;
    Reference<FileSystem> fs2 = ms()->resolve(newpath, &pos2);
    if(!fs2.valid())
        return Errors::last = Errors::NO_SUCH_FILE;
    if(fs1.get() != fs2.get())
        return Errors::last = Errors::XFS_LINK;
    return fs1->link(oldpath + pos1, newpath + pos2);
}

Errors::Code VFS::unlink(const char *path) {
    size_t pos;
    Reference<FileSystem> fs = ms()->resolve(path, &pos);
    if(!fs.valid())
        return Errors::last = Errors::NO_SUCH_FILE;
    return fs->unlink(path + pos);
}

void VFS::print(OStream &os) {
    VPE::self().mountspace()->print(os);
}

}
