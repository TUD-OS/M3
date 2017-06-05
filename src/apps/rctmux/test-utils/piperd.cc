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
#include <base/CPU.h>

#include <m3/stream/Standard.h>

using namespace m3;

alignas(64) static char buffer[4096];

int main(int argc, char **argv) {
    if(argc < 2)
        exitmsg("Usage: " << argv[0] << " <cycles>");

    cycles_t cycles = IStringStream::read_from<cycles_t>(argv[1]);

    while(cin.read(buffer, sizeof(buffer)) > 0)
        CPU::compute(cycles);
    return 0;
}
