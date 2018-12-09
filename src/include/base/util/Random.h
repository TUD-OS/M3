/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#pragma once

#include <base/Types.h>

namespace m3 {

/**
 * Simple way of pseudo random number generation.
 *
 * Source: http://en.wikipedia.org/wiki/Linear_congruential_generator
 */
class Random {
public:
    /**
     * Inits the random number generator with given seed
     *
     * @param seed the seed
     */
    static void init(uint seed) {
        _last = seed;
    }

    /**
     * @return the next random number
     */
    static int get() {
        _last = _randa * _last + _randc;
        return (_last / 65536) % 32768;
    }

private:
    Random();

    static uint _randa;
    static uint _randc;
    static uint _last;
};

}
