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

#include <m3/com/GateStream.h>
#include <m3/session/M3FS.h>
#include <m3/vfs/RegularFile.h>

namespace m3 {

File *M3FS::open(const char *path, int perms) {
    int fd;
    // ensure that the message gets acked immediately.
    {
        Errors::Code res;
        GateIStream resp = send_receive_vmsg(_gate, OPEN, path, perms);
        resp >> res;
        if(res != Errors::NO_ERROR) {
            Errors::last = res;
            return nullptr;
        }
        resp >> fd;
    }
    return new RegularFile(fd, Reference<M3FS>(this), perms);
}

Errors::Code M3FS::stat(const char *path, FileInfo &info) {
    GateIStream reply = send_receive_vmsg(_gate, STAT, path);
    Errors::Code res;
    reply >> res;
    if(res != Errors::NO_ERROR)
        return res;
    reply >> info;
    return Errors::NO_ERROR;
}

int M3FS::fstat(int fd, FileInfo &info) {
    GateIStream reply = send_receive_vmsg(_gate, FSTAT, fd);
    Errors::Code res;
    reply >> res;
    if(res != Errors::NO_ERROR)
        return res;
    reply >> info;
    return Errors::NO_ERROR;
}

int M3FS::seek(int fd, off_t off, int whence, size_t &global, size_t &extoff, off_t &pos) {
    GateIStream reply = send_receive_vmsg(_gate, SEEK, fd, off, whence, global, extoff);
    Errors::Code res;
    reply >> res;
    if(res != Errors::NO_ERROR)
        return res;
    reply >> global >> extoff >> pos;
    return Errors::NO_ERROR;
}

Errors::Code M3FS::mkdir(const char *path, mode_t mode) {
    GateIStream reply = send_receive_vmsg(_gate, MKDIR, path, mode);
    Errors::Code res;
    reply >> res;
    return res;
}

Errors::Code M3FS::rmdir(const char *path) {
    GateIStream reply = send_receive_vmsg(_gate, RMDIR, path);
    Errors::Code res;
    reply >> res;
    return res;
}

Errors::Code M3FS::link(const char *oldpath, const char *newpath) {
    GateIStream reply = send_receive_vmsg(_gate, LINK, oldpath, newpath);
    Errors::Code res;
    reply >> res;
    return res;
}

Errors::Code M3FS::unlink(const char *path) {
    GateIStream reply = send_receive_vmsg(_gate, UNLINK, path);
    Errors::Code res;
    reply >> res;
    return res;
}

void M3FS::close(int fd, size_t extent, size_t off) {
    // wait for the reply because we want to get our credits back
    send_receive_vmsg(_gate, CLOSE, fd, extent, off);
}

}
