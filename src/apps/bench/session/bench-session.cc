/*
 * Copyright (C) 2017-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/session/ClientSession.h>
#include <m3/stream/Standard.h>
#include <m3/Syscalls.h>

using namespace m3;

static const uint COUNT = 32;

int main() {
    Syscalls::get().noop();

    cycles_t total = 0;

    for(uint i = 0; i < COUNT; ++i) {
        cycles_t begin = Time::start(0x1234);
        ClientSession sess("test");
        cycles_t end = Time::stop(0x1234);
        total += end - begin;
    }

    cout << "Per session creation: " << (total / COUNT) << " cycles\n";
    return 0;
}
