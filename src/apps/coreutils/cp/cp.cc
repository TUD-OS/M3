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

#include <m3/stream/Standard.h>
#include <m3/stream/FStream.h>
#include <m3/vfs/VFS.h>

using namespace m3;

alignas(64) static char buffer[4096];

static void copy(const char *src, const char *dst) {
    FStream out(dst, FILE_W | FILE_CREATE | FILE_TRUNC);
    if(out.error()) {
        errmsg("Opening/creating " << dst << " for writing failed");
        return;
    }

    FStream in(src, FILE_R);
    if(in.error()) {
        errmsg("Opening/creating " << src << " for reading failed");
        return;
    }

    size_t res;
    while((res = in.read(buffer, sizeof(buffer))) > 0)
        out.write(buffer, res);
}

static void add_filename(OStringStream &os, const char *path) {
    if(!path || !path[0])
        return;

    if((path[0] == '.' || path[0] == '/') && path[1] == '\0') {
        os << path;
        return;
    }
    if(path[0] == '.' && path[1] == '.' && path[2] == '\0') {
        os << path;
        return;
    }

    size_t len = strlen(path);
    while(len > 0 && path[len - 1] == '/')
        len--;
    size_t end = len;
    while(len > 0 && path[len - 1] != '/')
        len--;
    for(size_t i = len; i <= end; ++i)
        os << path[i];
}

int main(int argc, char **argv) {
    if(argc < 3)
        exitmsg("Usage: " << argv[0] << " <in>... <out>");

    FileInfo info;
    Errors::Code res = VFS::stat(argv[argc - 1], info);

    if(res == Errors::NONE && M3FS_ISDIR(info.mode)) {
        for(int i = 1; i < argc - 1; ++i) {
            OStringStream dst;
            dst << argv[argc - 1] << "/";
            add_filename(dst, argv[i]);
            copy(argv[i], dst.str());
        }
    }
    else {
        if(argc > 3)
            exitmsg("Last argument is no directory, but multiple source files given");
        if(VFS::stat(argv[1], info) != Errors::NONE)
            exitmsg("Stat for " << argv[1] << " failed");
        if(M3FS_ISDIR(info.mode))
            exitmsg("Second argument is no directory, but first is");

        copy(argv[1], argv[2]);
    }
    return 0;
}
