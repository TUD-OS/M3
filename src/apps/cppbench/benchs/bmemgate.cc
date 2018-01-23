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
#include <base/Panic.h>

#include <m3/stream/Standard.h>

#include <m3/vfs/FileRef.h>

#include "../cppbench.h"

using namespace m3;

alignas(64) static char buf[8192];

NOINLINE static void read() {
    MemGate mgate = MemGate::create_global(8192, MemGate::R);

    Profile pr;
    cout << "8K: " << pr.run_with_id([&mgate] {
        mgate.read(buf, sizeof(buf), 0);
        if(Errors::occurred())
            PANIC("read failed");
    }, 0x40) << "\n";
}

NOINLINE static void write() {
    MemGate mgate = MemGate::create_global(8192, MemGate::W);

    Profile pr;
    cout << "8K: " << pr.run_with_id([&mgate] {
        mgate.write(buf, sizeof(buf), 0);
        if(Errors::occurred())
            PANIC("write failed");
    }, 0x41) << "\n";
}

void bmemgate() {
    RUN_BENCH(read);
    RUN_BENCH(write);
}
