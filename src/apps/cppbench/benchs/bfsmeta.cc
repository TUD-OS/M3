/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
#include <base/util/Profile.h>
#include <base/Panic.h>

#include <m3/stream/Standard.h>

#include <m3/vfs/VFS.h>

#include "../cppbench.h"

using namespace m3;

NOINLINE static void stat() {
    Profile pr(32, 4);

    cout << "Stat in root dir: " << pr.run_with_id([] {
        FileInfo info;
        if(VFS::stat("/large.txt", info) != Errors::NONE)
            PANIC("stat for /large.txt failed");
    }, 0x80) << "\n";

    cout << "Stat in sub dir: " << pr.run_with_id([] {
        FileInfo info;
        if(VFS::stat("/finddata/dir/dir-1/32.txt", info) != Errors::NONE)
            PANIC("stat for /finddata/dir/dir-1/32.txt failed");
    }, 0x81) << "\n";
}

void bfsmeta() {
    RUN_BENCH(stat);
}
