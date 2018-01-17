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

#include <accel/stream/StreamAccel.h>

namespace accel {

/**
 * The stream accelerators perform an algorithm over a stream of data, i.e., they read data,
 * manipulate it and write it to somewhere else.
 */
class Stream {
public:
    /**
     * Instantiates the stream accelerator
     *
     * @param isa the isa to use (fft or toupper)
     */
    explicit Stream(m3::PEISA isa);
    ~Stream();

    /**
     * Pumps <in> through the accelerator and stores the result into <out>. The slow version performs
     * the reading and writing from/to the file in software.
     *
     * @param in the input file
     * @param out the output file
     * @param bufsize the buffer size to use (<= StreamAccel::BUF_MAX_SIZE)
     * @return the number of bytes of the hash (or 0 on error)
     */
    m3::Errors::Code execute(m3::File *in, m3::File *out, size_t bufsize);
    m3::Errors::Code execute_slow(m3::File *in, m3::File *out, size_t bufsize);

private:
    void sendInit(size_t bufsize, size_t outsize, size_t reportsize);
    uint64_t sendRequest(uint64_t off, uint64_t len);

    StreamAccelVPE *_accel;
    m3::RecvGate _rgate;
    m3::RecvGate _argate;
    m3::SendGate _asgate;
    m3::SendGate _sgate;
};

}
