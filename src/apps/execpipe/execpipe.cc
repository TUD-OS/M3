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

#include <m3/cap/VPE.h>
#include <m3/util/Profile.h>
#include <m3/service/M3FS.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/pipe/PipeFS.h>
#include <m3/pipe/Pipe.h>
#include <m3/Log.h>

using namespace m3;

int main(int argc, char **argv) {
    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Mounting root-fs failed");
    VFS::mount("/pipe/", new PipeFS());

    if(argc < 4) {
        Serial::get() << "Usage: " << argv[0] << " <prog1> <prog2> <file>\n";
        return 1;
    }

    cycles_t start = Profile::start(0xA);

    VPE writer("writer");
    VPE reader("reader");
    Pipe pipe(reader, writer, 16 * 1024);

    {
        String path = pipe.get_path('w', "/pipe/");
        const char *args[] = {argv[1], argv[3], path.c_str()};
        writer.delegate_mounts();
        writer.exec(ARRAY_SIZE(args), args);
    }
    {
        String path = pipe.get_path('r', "/pipe/");
        const char *args[] = {argv[2], path.c_str()};
        reader.delegate_mounts();
        reader.exec(ARRAY_SIZE(args), args);
    }

    reader.wait();
    writer.wait();

    cycles_t end = Profile::stop(0xA);
    Serial::get() << "Total time: " << (end - start) << "\n";
    return 0;
}
