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

#include "lte.h"

using namespace m3;

int main(int argc, char **argv) {
    if(argc < 5)
        exitmsg("Usage: " << argv[0] << " <in> <mode> <num> <repeats>");

    if(VFS::mount("/", "m3fs") != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount filesystem\n");
    }

    const char *in = argv[1];
    // Mode mode = static_cast<Mode>(IStringStream::read_from<int>(argv[2]));
    size_t num = IStringStream::read_from<size_t>(argv[3]);
    int repeats = IStringStream::read_from<int>(argv[4]);

    for(int i = 0; i < repeats; ++i) {
        // open file
        fd_t infd = VFS::open(in, FILE_R);
        if(infd == FileTable::INVALID)
            exitmsg("Unable to open " << in);

        File *fin = VPE::self().fds()->get(infd);

        // if(mode == Mode::INDIR)
        //     chain_indirect(fin, fout, num, comptime);
        // else if(mode == Mode::DIR_MULTI)
        //     chain_direct_multi(fin, fout, num, comptime, Mode::DIR);
        // else
            chain_direct(fin, num);

        VFS::close(infd);
    }
    return 0;
}
