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

#include <base/util/Profile.h>

#include <m3/session/M3FS.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/Syscalls.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[4096];

int main(int argc, char **argv) {
    if(argc < 2)
        exitmsg("Usage: " << argv[0] << " <filename>");

    cycles_t start1 = Profile::start(0);
    if(VFS::mount("/", new M3FS("m3fs")) != Errors::NONE)
        exitmsg("Mounting root-fs failed");

    FileRef file(argv[1], FILE_R);
    if(Errors::occurred())
        exitmsg("open of " << argv[1] << " failed");
    cycles_t end1 = Profile::stop(0);

    cycles_t start2 = Profile::start(1);
    ssize_t count;
    size_t total = 0;
    unsigned checksum = 0;
    while((count = file->read(buffer, sizeof(buffer))) > 0) {
        total += count;
        unsigned *b = (unsigned*)buffer;
        unsigned *e = b + count / sizeof(unsigned);
        while(b < e)
            checksum += *b++;
    }
    cycles_t end2 = Profile::stop(1);

    cout << "Read " << total << " bytes; checksum=" << checksum << "\n";
    cout << "Setup time: " << (end1 - start1) << "\n";
    cout << "Read+checksum time: " << (end2 - start2) << "\n";
    return 0;
}
