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

namespace hash {

class Hash {
    class Accel {
    public:
        virtual ~Accel() {
        }

        virtual m3::VPE &get() = 0;
        virtual uintptr_t getRBAddr() = 0;
    };

    class AccelIMem : public Accel {
    public:
        explicit AccelIMem();

        m3::VPE &get() override {
            return _vpe;
        }
        uintptr_t getRBAddr() override;

    private:
        m3::VPE _vpe;
    };

    class AccelEMem : public Accel {
    public:
        explicit AccelEMem();

        m3::VPE &get() override {
            return _vpe;
        }
        uintptr_t getRBAddr() override;

    private:
        m3::VPE _vpe;
    };

    static const uint EPID          = 7;
    static const size_t RB_SIZE     = 1024;

public:
    static const size_t BUF_SIZE    = 4096;
    static const size_t BUF_ADDR    = PAGE_SIZE;

    enum Algorithm {
        SHA1,
        SHA224,
        SHA256,
        SHA384,
        SHA512,
        COUNT
    };

    explicit Hash();
    ~Hash();

    size_t get(Algorithm algo, const void *data, size_t len, void *res, size_t max);

private:
    static Accel *get_accel();

    Accel *_accel;
    m3::RecvBuf _rbuf;
    m3::RecvGate _rgate;
    m3::SendGate _send;
};

}
