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

namespace m3 {

class Hash : public Session {
public:
    static const size_t BUF_SIZE = 4096;

    enum Algorithm {
        SHA1,
        SHA224,
        SHA256,
        SHA384,
        SHA512,
        COUNT
    };

    explicit Hash(const String &service)
        : Session(service),
          _rbuf(RecvBuf::create(VPE::self().alloc_ep(), nextlog2<256>::val, 0)),
          _rgate(RecvGate::create(&_rbuf)),
          _send(SendGate::bind(obtain(2).start() + 0, &_rgate)),
          _mem(MemGate::bind(_send.sel() + 1)) {
    }

    const SendGate &sendgate() const {
        return _send;
    }
    const MemGate &memgate() const {
        return _mem;
    }

    size_t get(Algorithm algo, const void *data, size_t len, void *res, size_t max);

private:
    RecvBuf _rbuf;
    RecvGate _rgate;
    SendGate _send;
    MemGate _mem;
};

}
