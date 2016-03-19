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
#include <base/Log.h>

#include <m3/session/M3FS.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/Syscalls.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[4096];

int main(int argc, char **argv) {
    if(argc < 2) {
        Serial::get() << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }

    cycles_t start1 = Profile::start(0);
    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Mounting root-fs failed");

    FileRef file(argv[1], FILE_R);
    if(Errors::occurred())
        PANIC("open of " << argv[1] << " failed (" << Errors::last << ")");
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

    Serial::get() << "Read " << total << " bytes; checksum=" << checksum << "\n";
    Serial::get() << "Setup time: " << (end1 - start1) << "\n";
    Serial::get() << "Read+checksum time: " << (end2 - start2) << "\n";
    return 0;
}
