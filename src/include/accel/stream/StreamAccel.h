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

namespace accel {

/**
 * Base class for the stream accelerators
 */
class StreamAccelVPE : public m3::VPE {
public:
    static const size_t MSG_SIZE    = 64;
    static const size_t RB_SIZE     = MSG_SIZE * 8;
    static const capsel_t RGATE_SEL = 2;
    static const capsel_t SGATE_SEL = 3;
    static const size_t EP_RECV     = 7;
    static const size_t EP_INPUT    = 8;
    static const size_t EP_OUTPUT   = 9;
    static const size_t EP_SEND     = 10;

    static const size_t BUF_MAX_SIZE;
    static const size_t BUF_ADDR;

    enum class Command {
        INIT,
        UPDATE,
    };

    struct InitCommand {
        uint64_t cmd;
        uint64_t buf_size;
        uint64_t out_size;
        uint64_t report_size;
        uint64_t comp_time;
    } PACKED;

    struct UpdateCommand {
        uint64_t cmd;
        uint64_t off;
        uint64_t len;
        uint64_t eof;
    } PACKED;

    explicit StreamAccelVPE(const m3::PEDesc &desc, const char *pager, bool muxable);

    /**
     * Creates an accelerator, depending on which exists
     *
     * @param isa the ISA (fft, toupper)
     * @return the accelerator
     */
    static StreamAccelVPE *create(m3::PEISA isa);

    /**
     * @return the address of the receive buffer
     */
    virtual uintptr_t getRBAddr() = 0;
};

/**
 * A stream accelerator with SPM, i.e., internal memory.
 */
class StreamIAccelVPE : public StreamAccelVPE {
public:
    explicit StreamIAccelVPE(m3::PEISA isa, bool muxable);

    uintptr_t getRBAddr() override;
};

/**
 * A stream accelerator with cache, i.e., external memory.
 */
class StreamEAccelVPE : public StreamAccelVPE {
public:
    explicit StreamEAccelVPE(m3::PEISA isa, bool muxable);

    uintptr_t getRBAddr() override;
};

}
