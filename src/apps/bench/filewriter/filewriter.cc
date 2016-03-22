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

#include <base/stream/IStringStream.h>
#include <base/util/Profile.h>

#include <m3/session/M3FS.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/Syscalls.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[4096];

int main(int argc, char **argv) {
    if(argc < 3)
        exitmsg("Usage: " << argv[0] << " <filename> <size>");

    size_t size = IStringStream::read_from<size_t>(argv[2]);
    for(size_t i = 0; i < sizeof(buffer); ++i)
        buffer[i] = i & 0xFF;

    cycles_t end1,start2,end2,start3;
    cycles_t start1 = Profile::start(0);
    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        exitmsg("Mounting root-fs failed");

    {
        FileRef file(argv[1], FILE_W | FILE_TRUNC | FILE_CREATE);
        if(Errors::occurred())
            exitmsg("open of " << argv[1] << " failed");
        end1 = Profile::stop(0);

        start2 = Profile::start(1);
        for(size_t total = 0; total < size; ) {
            ssize_t count = file->write(buffer, sizeof(buffer));
            if(count < 0)
                exitmsg("Writing failed");
            total += count;
        }
        end2 = Profile::stop(1);

        start3 = Profile::start(2);
    }
    cycles_t end3 = Profile::stop(2);

    cout << "Setup time: " << (end1 - start1) << "\n";
    cout << "Write time: " << (end2 - start2) << "\n";
    cout << "Shutdown time: " << (end3 - start3) << "\n";
    return 0;
}
