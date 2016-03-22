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
#include <m3/stream/Standard.h>
#include <m3/pipe/Pipe.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/VPE.h>

using namespace m3;

int main(int argc, char **argv) {
    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Mounting root-fs failed");

    if(argc < 4) {
        Serial::get() << "Usage: " << argv[0] << " <prog1> <prog2> <file>\n";
        return 1;
    }

    cycles_t start = Profile::start(0xA);

    VPE writer("writer");
    VPE reader("reader");
    Pipe pipe(reader, writer, 16 * 1024);

    reader.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
    reader.obtain_fds();

    reader.mountspace(*VPE::self().mountspace());
    reader.obtain_mountspace();

    writer.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe.writer_fd()));
    writer.obtain_fds();

    writer.mountspace(*VPE::self().mountspace());
    writer.obtain_mountspace();

    {
        const char *args[] = {argv[1]};
        writer.exec(ARRAY_SIZE(args), args);
    }
    {
        const char *args[] = {argv[2]};
        reader.exec(ARRAY_SIZE(args), args);
    }

    pipe.close_reader();
    pipe.close_writer();
    reader.wait();
    writer.wait();

    cycles_t end = Profile::stop(0xA);
    Serial::get() << "Total time: " << (end - start) << "\n";
    return 0;
}
