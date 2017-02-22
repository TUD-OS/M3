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
#include <m3/session/Session.h>
#include <m3/vfs/File.h>

#include <hash/Accel.h>

namespace hash {

class Hash {
public:
    typedef Accel::Algorithm Algorithm;

    explicit Hash();
    ~Hash();

    bool start(bool autonomous, Algorithm algo) {
        _lastmem = m3::ObjCap::INVALID;
        return sendRequest(Accel::Command::INIT, autonomous, algo) == 1;
    }
    bool update(capsel_t mem, size_t offset, size_t len);
    bool update(const void *data, size_t len);
    size_t finish(void *res, size_t max);

    size_t get(Algorithm algo, m3::File *file, void *res, size_t max);
    size_t get_auto(Algorithm algo, m3::File *file, void *res, size_t max);
    size_t get(Algorithm algo, const void *data, size_t len, void *res, size_t max);

private:
    uint64_t sendRequest(Accel::Command cmd, uint64_t arg1, uint64_t arg2);

    Accel *_accel;
    capsel_t _lastmem;
    m3::RecvGate _rgate;
    m3::RecvGate _srgate;
    m3::SendGate _sgate;
};

}
