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

#include <m3/session/M3FS.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>

#include "common/traceplayer.h"
#include "platform.h"

using namespace m3;

int main(int argc, char **argv) {
    Platform::init(argc, argv);

    VFS::mount("/", "m3fs");

    // defaults
    long num_iterations = 4;
    bool keep_time   = true;
    bool make_ckpt   = false;

    // playback / revert to init-trace contents
    const char *prefix = "";
    if(argc > 1) {
        prefix = argv[1];
        VFS::mkdir(prefix, 0755);
    }
    TracePlayer player(prefix);

    // print parameters for reference
    cout << "VPFS trace_bench started ["
         << "n=" << num_iterations << ","
         << "keeptime=" << (keep_time   ? "yes" : "no")
         << "]\n";

    for(int i = 0; i < num_iterations; ++i)
        player.play(keep_time, make_ckpt);

    cout << "VPFS trace_bench benchmark terminated\n";

    // done
    Platform::shutdown();
    return 0;
}
