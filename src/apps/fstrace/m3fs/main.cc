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

#include <m3/Common.h>
#include <m3/service/M3FS.h>
#include <m3/vfs/VFS.h>
#include <m3/Log.h>
#include <cstring>

#include "common/traceplayer.h"
#include "platform.h"

using namespace m3;

int main(int argc, char **argv) {
    Platform::init(argc, argv);

    VFS::mount("/", new M3FS("m3fs"));

    // defaults
    long num_iterations = 1;
    bool keep_time   = true;
    bool make_ckpt   = false;

    // playback / revert to init-trace contents
    const char *prefix = "";
    if(argc > 2) {
        prefix = argv[1];
        VFS::mkdir(prefix, 0755);
    }
    TracePlayer player(prefix);

    // print parameters for reference
    Serial::get() << "VPFS trace_bench started ["
        << "n=" << num_iterations << ","
        << "keeptime=" << (keep_time   ? "yes" : "no")
        << "]\n";

    player.play(keep_time, make_ckpt);

    Serial::get() << "VPFS trace_bench benchmark terminated\n";

    // done
    Platform::shutdown();
    return 0;
}
