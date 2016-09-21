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
#include <base/util/Random.h>

#include <m3/stream/Standard.h>

using namespace m3;

alignas(DTU_PKG_SIZE) static char buffer[4096];

int main(int argc, char **argv) {
    if(argc != 3)
        exitmsg("Usage: " << argv[0] << " <count> <seed>");

    size_t count = IStringStream::read_from<size_t>(argv[1]);

    uint seed = IStringStream::read_from<size_t>(argv[2]);
    Random::init(seed);

    while(count > 0) {
        size_t amount = Math::min(count, ARRAY_SIZE(buffer));

        for(size_t i = 0; i < amount; ++i)
            buffer[i] = 'a' + (Random::get() % 26);
        cout.write(buffer, amount);

        count -= amount;
    }
    return 0;
}
