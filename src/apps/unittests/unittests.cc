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

using namespace m3;

static const char *progs[] = {
#if defined(__host__)
    "/bin/unittests-dtu",
#endif
    "/bin/unittests-stream",
    "/bin/unittests-fs",
    "/bin/unittests-fs2",
    "/bin/unittests-misc",
};

int main() {
    if(VFS::mount("/", new M3FS("m3fs")) < 0) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount m3fs as root-fs");
    }

    int succ = 0;
    for(size_t i = 0; i < ARRAY_SIZE(progs); ++i) {
        VPE t("tests");
        const char *args[] = {progs[i]};

        t.mountspace(*VPE::self().mountspace());
        t.obtain_mountspace();

        t.exec(ARRAY_SIZE(args), args);
        uint32_t res = t.wait();
        if((res >> 16) != 0)
            succ += (res & 0xFFFF) == (res >> 16);
    }
    cout << "---------------------------------------\n";
    cout << "\033[1mIn total: " << (succ == ARRAY_SIZE(progs) ? "\033[1;32m" : "\033[1;31m")
            << succ << "\033[1m of " << ARRAY_SIZE(progs) << " testsuites successfull\033[0;m\n";
    cout << "---------------------------------------\n";
    return 0;
}
