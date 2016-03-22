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
#include <base/util/Profile.h>
#include <base/DTU.h>

#include <m3/com/MemGate.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/Dir.h>
#include <m3/Syscalls.h>

using namespace m3;

int main(int argc, char **argv) {
    cout << "Got " << argc << " arguments:\n";
    for(int i = 0; i < argc; ++i)
        cout << "  " << i << ": " << argv[i] << "\n";

    const char *dirname = "/bin";
    Dir dir(dirname);
    if(!Errors::occurred()) {
        cout << "Listing dir " << dirname << "...\n";
        Dir::Entry e;
        while(dir.readdir(e))
            cout << " Found " << e.name << " -> " << e.nodeno << "\n";
    }

    for(int i = 0; i < 10; ++i)
        cout << "Hello @ " << Profile::start() << "!\n";
    return 0;
}
