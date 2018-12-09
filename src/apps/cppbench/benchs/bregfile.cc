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

#include <m3/vfs/FileRef.h>

#include "../cppbench.h"

using namespace m3;

alignas(64) static char buf[8192];

NOINLINE static void read() {
    Profile pr(2, 1);

    cout << "2 MiB file with 8K buf: " << pr.run_with_id([] {
        FileRef file("/data/2048k.txt", FILE_R);
        if(Errors::occurred())
            PANIC("Unable to open file '/data/2048k.txt'");

        ssize_t amount;
        while((amount = file->read(buf, sizeof(buf))) > 0)
            ;
    }, 0x30) << "\n";
}

NOINLINE static void write() {
    const size_t SIZE = 2 * 1024 * 1024;
    Profile pr(2, 1);

    cout << "2 MiB file with 8K buf: " << pr.run_with_id([] {
        FileRef file("/newfile", FILE_W | FILE_TRUNC | FILE_CREATE);
        if(Errors::occurred())
            PANIC("Unable to open file '/newfile'");

        size_t total = 0;
        while(total < SIZE) {
            ssize_t amount = file->write(buf, sizeof(buf));
            if(amount <= 0)
                PANIC("Unable to write to file");
            total += static_cast<size_t>(amount);
        }
    }, 0x31) << "\n";
}

void bregfile() {
    RUN_BENCH(read);
    RUN_BENCH(write);
}
