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

#include <base/util/Time.h>
#include <base/stream/IStringStream.h>

#include <m3/session/M3FS.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/Syscalls.h>

using namespace m3;

alignas(64) static char buffer[8192];

int main(int argc, char **argv) {
    if(argc < 3)
        exitmsg("Usage: " << argv[0] << " <in> <out> [<repeats>]");

    // TODO temporary fix to support different use-cases without complicating debugging.
    // Because if we require that the mountspace is configured by the parent (which is the goal),
    // we can only use it via VPE::exec, but this would not allow to debug it in a convenient way.
    if(VFS::mount("/", "m3fs") != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Mounting root-fs failed");
    }

    int repeats = argc > 3 ? IStringStream::read_from<int>(argv[3]) : 1;

    for(int i = 0; i < repeats; ++i) {
        FileRef input(argv[1], FILE_R);
        if(Errors::occurred())
            exitmsg("open of " << argv[1] << " failed");

        FileRef output(argv[2], FILE_W | FILE_TRUNC | FILE_CREATE);
        if(Errors::occurred())
            exitmsg("open of " << argv[2] << " failed");

        cycles_t start = Time::start(1);
        ssize_t count;
        while((count = input->read(buffer, sizeof(buffer))) > 0)
            output->write(buffer, static_cast<size_t>(count));
        cycles_t end = Time::stop(1);

        cout << "Copy time: " << (end - start) << " cycles\n";
    }
    return 0;
}
