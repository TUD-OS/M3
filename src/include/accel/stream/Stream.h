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
     * Convenience method that calls start, update and finish for the given file. The method get()
     * works non-autonomously and get_auto() works autonomously.
     *
     * @param algo the hash algorithm to use
     * @param file the file to generate the hash for
     * @param res the array to store the hash to
     * @param max the size of the array
     * @return the number of bytes of the hash (or 0 on error)
     */
    m3::Errors::Code execute(m3::File *in, m3::File *out);
    m3::Errors::Code execute_slow(m3::File *in, m3::File *out);

private:
    uint64_t sendRequest(uint64_t inoff, uint64_t outoff, uint64_t len, bool autonomous);

    StreamAccel *_accel;
    m3::RecvGate _rgate;
    m3::RecvGate _srgate;
    m3::SendGate _sgate;
};

}
