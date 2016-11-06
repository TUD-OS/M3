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

#include <m3/session/Session.h>
#include <m3/com/MemGate.h>
#include <m3/com/SendGate.h>

#include <hash/Accel.h>

namespace m3 {

class Hash : public Session {
public:
    static const size_t BUF_SIZE = 4096;

    enum Operation {
        CREATE_HASH,
        COUNT
    };

    typedef hash::Accel::Algorithm Algorithm;

    explicit Hash(const String &service)
        : Session(service),
          _rgate(RecvGate::create(nextlog2<256>::val, nextlog2<256>::val)),
          _sgate(SendGate::bind(obtain(1).start(), &_rgate)),
          _mem(MemGate::bind(obtain(1).start())) {
        _rgate.activate();
    }

    size_t get(Algorithm algo, const void *data, size_t len, void *res, size_t max) {
        assert(len <= BUF_SIZE);
        _mem.write(data, len, 0);

        hash::Accel::Request req;
        req.algo = algo;
        req.len = len;
        GateIStream is = send_receive_vmsg(_sgate, CREATE_HASH, req);

        uint64_t count;
        is >> count;

        if(count == 0)
            return 0;
        memcpy(res, is.buffer() + sizeof(uint64_t), Math::min(max, static_cast<size_t>(count)));
        return count;
    }

private:
    RecvGate _rgate;
    SendGate _sgate;
    MemGate _mem;
};

}
