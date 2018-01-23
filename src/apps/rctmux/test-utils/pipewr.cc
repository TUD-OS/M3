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

#include <base/stream/IStringStream.h>
#include <base/util/Time.h>
#include <base/CPU.h>

#include <m3/stream/Standard.h>

using namespace m3;

alignas(64) static char buffer[8192];

int main(int argc, char **argv) {
    if(argc < 3)
        exitmsg("Usage: " << argv[0] << " <bytes> <cycles>");

    size_t bytes = IStringStream::read_from<size_t>(argv[1]);
    cycles_t cycles = IStringStream::read_from<cycles_t>(argv[2]);

    Time::start(0x1235);

    while(bytes > 0) {
        CPU::compute(cycles);

        size_t amount = Math::min(bytes, sizeof(buffer));
        cout.write(buffer, amount);

        bytes -= amount;
    }

    Time::stop(0x1235);
    return 0;
}
