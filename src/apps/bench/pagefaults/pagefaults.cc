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

static const size_t COUNT       = 8;
static const size_t PAGES       = 64;

static void do_bench(volatile char *data) {
    for(size_t i = 0; i < PAGES; ++i)
        data[i * PAGE_SIZE] = i;
}

int main() {
    if(!VPE::self().pager())
        exitmsg("No pager");

    cycles_t anon = 0;
    for(size_t i = 0; i < COUNT; ++i) {
        uintptr_t virt = 0x30000000;
        Errors::Code res = VPE::self().pager()->map_anon(&virt, PAGES * PAGE_SIZE,
            Pager::READ | Pager::WRITE, 0);
        if(res != Errors::NONE)
            exitmsg("Unable to map anonymous memory");

        cycles_t start = Profile::start(0);
        do_bench(reinterpret_cast<char*>(virt));
        cycles_t end = Profile::stop(0);
        anon += end - start;

        VPE::self().pager()->unmap(virt);
    }

    cycles_t file = 0;
    for(size_t i = 0; i < COUNT; ++i) {
        FileRef f("/zeros.bin", FILE_R);
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to open /zeros.bin");

        RegularFile *rfile = static_cast<RegularFile*>(&*f);
        uintptr_t virt = 0x31000000;
        Errors::Code res = VPE::self().pager()->map_ds(&virt, PAGES * PAGE_SIZE,
            Pager::READ, 0, *rfile->fs(), rfile->fd(), 0);
        if(res != Errors::NONE)
            exitmsg("Unable to map /test.txt");

        cycles_t start = Profile::start(1);
        do_bench(reinterpret_cast<char*>(virt));
        cycles_t end = Profile::stop(1);
        file += end - start;

        VPE::self().pager()->unmap(virt);
    }

    cout << "anon: " << (anon / COUNT) << "\n";
    cout << "file: " << (file / COUNT) << "\n";
    return 0;
}
