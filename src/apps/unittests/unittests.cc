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

#include <m3/vfs/VFS.h>
#include <m3/stream/Standard.h>
#include <m3/VPE.h>

#include "unittests.h"

using namespace m3;

int succeeded;
int failed;

int main() {
    if(VFS::mount("/", "m3fs") != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount m3fs as root-fs");
    }

#if defined(__host__)
    RUN_SUITE(tdtu);
#endif
    RUN_SUITE(tfsmeta);
    RUN_SUITE(tfs);
    RUN_SUITE(tbitfield);
    RUN_SUITE(theap);
    RUN_SUITE(tstream);

    cout << "---------------------------------------\n";
    cout << "\033[1mIn total: " << (failed == 0 ? "\033[1;32m" : "\033[1;31m")
            << succeeded << "\033[1m of " << (failed + succeeded) << " tests successfull\033[0;m\n";
    cout << "---------------------------------------\n";
    return 0;
}
