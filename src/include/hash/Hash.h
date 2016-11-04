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

#pragma once

#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>

#include <hash/Accel.h>

namespace hash {

class Hash {
    static const uintptr_t BUF_ADDR     = 0x3000;

public:
    typedef Accel::Algorithm Algorithm;

    explicit Hash();
    ~Hash();

    size_t get(Algorithm algo, const void *data, size_t len, void *res, size_t max);

private:
    Accel *_accel;
    m3::RecvGate _srgate;
    m3::RecvGate _crgate;
    m3::SendGate _send;
};

}
