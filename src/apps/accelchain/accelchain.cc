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

#include <base/Common.h>
#include <base/stream/IStringStream.h>
#include <base/util/Time.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>

#include "accelchain.h"

using namespace m3;

int main(int argc, char **argv) {
    if(argc < 6)
        exitmsg("Usage: " << argv[0] << " <in> <out> <mode> <comptime> <num>");

    if(VFS::mount("/", "m3fs") != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount filesystem\n");
    }

    const char *in = argv[1];
    const char *out = argv[2];
    Mode mode = static_cast<Mode>(IStringStream::read_from<int>(argv[3]));
    cycles_t comptime = IStringStream::read_from<cycles_t>(argv[4]);
    size_t num = IStringStream::read_from<size_t>(argv[5]);

    // open files
    fd_t infd = VFS::open(in, FILE_R);
    if(infd == FileTable::INVALID)
        exitmsg("Unable to open " << in);
    fd_t outfd = VFS::open(out, FILE_W | FILE_TRUNC | FILE_CREATE);
    if(outfd == FileTable::INVALID)
        exitmsg("Unable to open " << out);

    File *fin = VPE::self().fds()->get(infd);
    File *fout = VPE::self().fds()->get(outfd);

    if(mode == Mode::INDIR)
        chain_indirect(fin, fout, num, comptime);
    else
        chain_direct(fin, fout, num, comptime, mode);

    VFS::close(infd);
    VFS::close(outfd);
    return 0;
}
