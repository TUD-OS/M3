/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
#include <m3/vfs/VFS.h>

using namespace m3;

int main(int argc, char **argv) {
    if(argc < 2)
        exitmsg("Usage: " << argv[0] << " <file>...");

    for(int i = 1; i < argc; ++i) {
        FileInfo info;
        if(VFS::stat(argv[i], info) != Errors::NONE)
            exitmsg("stat of " << argv[i] << " failed");
        cout << "File   : " << argv[i] << "\n";
        cout << "devno  : " << info.devno << "\n";
        cout << "inode  : " << info.inode << "\n";
        cout << "mode   : " << fmt(info.mode, "0o", 5) << "\n";
        cout << "size   : " << info.size << "\n";
        cout << "links  : " << info.links << "\n";
        cout << "acctime: " << info.lastaccess << "\n";
        cout << "modtime: " << info.lastmod << "\n";
        cout << "extents: " << info.extents << "\n";
    }
    return 0;
}
