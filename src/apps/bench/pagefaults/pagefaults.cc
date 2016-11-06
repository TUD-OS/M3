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

#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/FileRef.h>
#include <m3/vfs/RegularFile.h>
#include <m3/VPE.h>

using namespace m3;

static const size_t PAGES       = 64;
static const size_t STEPWIDTH   = 1;

static cycles_t do_bench(uint no, volatile char *data) {
    cycles_t total = 0;
    for(size_t i = 0; i < PAGES; i += STEPWIDTH) {
        cycles_t start = Profile::start(no);
        UNUSED char d = data[i * PAGE_SIZE];
        cycles_t end = Profile::stop(no);
        total += end - start;
    }
    return total;
}

int main() {
    if(!VPE::self().pager())
        exitmsg("No pager");

    cycles_t anon, file;

    {
        uintptr_t virt = 0x30000000;
        Errors::Code res = VPE::self().pager()->map_anon(&virt, PAGES * PAGE_SIZE,
            Pager::READ, 0);
        if(res != Errors::NONE)
            exitmsg("Unable to map anonymous memory");

        anon = do_bench(0, reinterpret_cast<char*>(virt));
    }

    {
        FileRef f("/zeros.bin", FILE_R);
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to open /zeros.bin");

        RegularFile *rfile = static_cast<RegularFile*>(&*f);
        uintptr_t virt = 0x31000000;
        Errors::Code res = VPE::self().pager()->map_ds(&virt, PAGES * PAGE_SIZE,
            Pager::READ, 0, *rfile->fs(), rfile->fd(), 0);
        if(res != Errors::NONE)
            exitmsg("Unable to map /test.txt");

        file = do_bench(1, reinterpret_cast<char*>(virt));
    }

    cout << "anon/page: " << (anon / PAGES) << "\n";
    cout << "file/page: " << (file / PAGES) << "\n";
    return 0;
}
