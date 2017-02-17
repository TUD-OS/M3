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

#include <hash/Accel.h>

namespace hash {

class Hash {
    class Backend {
    public:
        virtual ~Backend() {
        }
    };

    class DirectBackend : public Backend {
    public:
        explicit DirectBackend();
        ~DirectBackend();

        Accel *accel;
        m3::RecvGate srgate;
    };

    class IndirectBackend : public Backend {
    public:
        explicit IndirectBackend(const char *service);

        m3::Session sess;
    };

public:
    typedef Accel::Algorithm Algorithm;

    explicit Hash();
    explicit Hash(const char *service);
    ~Hash();

    Accel *accel() {
        return static_cast<DirectBackend*>(_backend)->accel;
    }

    bool start(Algorithm algo) {
        return sendRequest(Accel::Command::INIT, algo) == 1;
    }
    bool update(const void *data, size_t len, bool write = true);
    size_t finish(void *res, size_t max);

    size_t get(Algorithm algo, const void *data, size_t len, void *res, size_t max);

private:
    uint64_t sendRequest(Accel::Command cmd, uint64_t arg);

    Backend *_backend;
    m3::RecvGate _rgate;
    m3::SendGate _sgate;
    m3::MemGate _mgate;
    size_t _memoff;
};

}
