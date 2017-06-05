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

#define BUFFER_SIZE     4096
#define EL_COUNT        (BUFFER_SIZE / sizeof(rand_type))

typedef uchar rand_type;

alignas(64) static rand_type buffer[EL_COUNT];

static unsigned get_rand() {
    static unsigned _last = 0x1234;
    static unsigned _randa = 1103515245;
    static unsigned _randc = 12345;
    _last = _randa * _last + _randc;
    return (_last / 65536) % 32768;
}

int main(int argc, char **argv) {
    if(argc != 2)
        exitmsg("Usage: " << argv[0] << " <count>");

    size_t count = IStringStream::read_from<size_t>(argv[1]);

    while(count > 0) {
        size_t amount = Math::min(count, ARRAY_SIZE(buffer));

        for(size_t i = 0; i < amount; ++i)
            buffer[i] = get_rand() * get_rand();
        cout.write(buffer, amount * sizeof(rand_type));

        count -= amount;
    }
    return 0;
}
