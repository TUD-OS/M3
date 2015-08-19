/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/stream/OStream.h>
#include <execinfo.h>

namespace m3 {

class Backtrace;
OStream &operator<<(OStream &os, const Backtrace &bt);

class Backtrace {
    friend OStream &operator<<(OStream &os, const Backtrace &bt);

    static const size_t MAX_DEPTH       = 32;

public:
    explicit Backtrace() : _trace(), _count() {
        _count = backtrace(_trace, MAX_DEPTH);
    }

    void *_trace[MAX_DEPTH];
    size_t _count;
};

}
