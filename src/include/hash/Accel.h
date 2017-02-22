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

#include <m3/VPE.h>

namespace hash {

class Accel {
public:
    static const uint RBUF          = 2;
    static const uint RECV_EP       = 7;
    static const uint MEM_EP        = 8;
    static const uint DATA_EP       = 9;
    static const size_t RB_SIZE     = 64;

    static const size_t BUF_SIZE;
    static const size_t BUF_ADDR;
    static const size_t STATE_SIZE;
    static const size_t STATE_ADDR;

    enum Algorithm {
        SHA1,
        SHA224,
        SHA256,
        SHA384,
        SHA512,
        COUNT
    };

    enum Command {
        INIT,
        UPDATE,
        FINISH,
    };

    struct Request {
        uint64_t cmd;
        uint64_t arg1;
        uint64_t arg2;
    } PACKED;

    static Accel *create();

    virtual ~Accel() {
    }

    virtual m3::VPE &vpe() = 0;
    virtual uintptr_t getRBAddr() = 0;
};

class AccelIMem : public Accel {
public:
    explicit AccelIMem(bool muxable);

    m3::VPE &vpe() override {
        return _vpe;
    }
    uintptr_t getRBAddr() override;

private:
    m3::VPE _vpe;
    m3::MemGate _spm;
};

class AccelEMem : public Accel {
public:
    explicit AccelEMem(bool muxable);

    m3::VPE &vpe() override {
        return _vpe;
    }
    uintptr_t getRBAddr() override;

private:
    m3::VPE _vpe;
};

}
