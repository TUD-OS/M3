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

#include <m3/com/MemGate.h>
#include <m3/stream/Standard.h>
#include <m3/Syscalls.h>

using namespace m3;

#define WARMUP  50
#define COUNT   100

int main() {
    cycles_t total = 0;

    // do some warmup
    for(int i = 0; i < WARMUP; ++i)
        Syscalls::get().noop();

    cout << "Starting...\n";
    for(int i = 0; i < COUNT; ++i) {
        cycles_t start = Profile::start(0);
        Syscalls::get().noop();
        cycles_t end = Profile::stop(0);
        total += end - start;
    }
    cout << "Per syscall: " << (total / COUNT) << "\n";
    return 0;
}
