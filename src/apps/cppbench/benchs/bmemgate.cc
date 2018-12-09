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

static const size_t SIZE = 2 * 1024 * 1024;

alignas(64) static char buf[8192];

NOINLINE static void read() {
    MemGate mgate = MemGate::create_global(8192, MemGate::R);

    Profile pr(2, 1);;
    cout << "2 MiB with 8K buf: " << pr.run_with_id([&mgate] {
        size_t total = 0;
        while(total < SIZE) {
            mgate.read(buf, sizeof(buf), 0);
            if(Errors::occurred())
                PANIC("read failed");
            total += sizeof(buf);
        }
    }, 0x40) << "\n";
}

NOINLINE static void write() {
    MemGate mgate = MemGate::create_global(8192, MemGate::W);

    Profile pr(2, 1);
    cout << "2 MiB with 8K buf: " << pr.run_with_id([&mgate] {
        size_t total = 0;
        while(total < SIZE) {
            mgate.write(buf, sizeof(buf), 0);
            if(Errors::occurred())
                PANIC("write failed");
            total += sizeof(buf);
        }
    }, 0x41) << "\n";
}

void bmemgate() {
    RUN_BENCH(read);
    RUN_BENCH(write);
}
