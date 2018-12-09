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

#include "loop.h"

static rand_type get_rand() {
    static unsigned _last = 0x1234;
    static unsigned _randa = 1103515245;
    static unsigned _randc = 12345;
    _last = (_randa * _last) + _randc;
    return (_last / 65536) % 32768;
}

void generate(rand_type *buffer, unsigned long amount) {
    rand_type total = 0;
    for(unsigned long i = 0; i < amount; ++i)
        total += get_rand();
    buffer[0] = total;
}
