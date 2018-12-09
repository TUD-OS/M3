/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/util/Time.h>

#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/FileRef.h>
#include <m3/VPE.h>

using namespace m3;

static const size_t COUNT       = 4;
static const size_t PAGES       = 64;

static cycles_t do_warmup(volatile char *data, unsigned id1) {
    cycles_t start = Time::start(id1);
    for(size_t i = 0; i < PAGES; ++i)
        data[i * PAGE_SIZE] = i;
    return Time::stop(id1) - start;
}

static cycles_t do_bench(volatile char *data, unsigned id1, unsigned id2) {
    cycles_t start = Time::start(id1);
    for(size_t i = 0; i < PAGES; ++i) {
        Time::start(id2);
        data[i * PAGE_SIZE] = i;
        Time::stop(id2);
    }
    return Time::stop(id1) - start;
}

int main(int argc, char **argv) {
    if(argc < 2)
        exitmsg("Usage: " << argv[0] << " (anon|file)");
    if(!VPE::self().pager())
        exitmsg("No pager");

    if(strcmp(argv[1], "anon") == 0) {
        cycles_t anon = 0;
        for(size_t i = 0; i < COUNT; ++i) {
            goff_t virt = 0x30000000;
            Errors::Code res = VPE::self().pager()->map_anon(&virt, PAGES * PAGE_SIZE,
                                                             Pager::READ | Pager::WRITE, 0);
            if(res != Errors::NONE)
                exitmsg("Unable to map anonymous memory");

            if(i < COUNT - 1)
                anon += do_warmup(reinterpret_cast<char*>(virt), 0xFF);
            else
                anon += do_bench(reinterpret_cast<char*>(virt), 0xFF, 0);

            VPE::self().pager()->unmap(virt);
        }
        cout << "anon: " << (anon / (COUNT * PAGES)) << "\n";
    }

    if(strcmp(argv[1], "file") == 0) {
        cycles_t file = 0;
        for(size_t i = 0; i < COUNT; ++i) {
            FileRef f("/zeros.bin", FILE_RW);
            if(Errors::last != Errors::NONE)
                exitmsg("Unable to open /zeros.bin");

            const GenericFile *rfile = static_cast<const GenericFile*>(&*f);
            goff_t virt = 0x31000000;
            Errors::Code res = VPE::self().pager()->map_ds(&virt, PAGES * PAGE_SIZE,
                                                           Pager::READ | Pager::WRITE, 0,
                                                           rfile->sess(), 0);
            if(res != Errors::NONE)
                exitmsg("Unable to map /test.txt");

            if(i < COUNT - 1)
                file += do_warmup(reinterpret_cast<char*>(virt), 0xFF);
            else
                file += do_bench(reinterpret_cast<char*>(virt), 0xFF, 1);

            VPE::self().pager()->unmap(virt);
        }
        cout << "file: " << (file / (COUNT * PAGES)) << "\n";
    }
    return 0;
}
