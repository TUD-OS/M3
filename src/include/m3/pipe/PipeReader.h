/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/Common.h>
#include <m3/pipe/Pipe.h>
#include <m3/GateStream.h>

namespace m3 {

/**
 * Reads from a previously constructed pipe.
 */
class PipeReader {
public:
    /**
     * Constructs a pipe-reader for given pipe.
     *
     * @param p the pipe
     */
    explicit PipeReader(const Pipe &p) : PipeReader(p.caps(), p.receive_chan()) {
    }
    explicit PipeReader(capsel_t caps, size_t rchan)
        : _mgate(MemGate::bind(caps)),
          _rbuf(RecvBuf::create(rchan,
            nextlog2<Pipe::MSG_BUF_SIZE>::val, nextlog2<Pipe::MSG_SIZE>::val, 0)),
          _rgate(RecvGate::create(&_rbuf)),
          _pos(), _rem(), _pkglen(-1), _eof(0), _is(_rgate) {
    }
    /**
     * Sends EOF
     */
    ~PipeReader() {
        send_eof();
    }

    /**
     * @return true if there is currently data to read
     */
    bool has_data() const {
        return _rem > 0 || ChanMng::get().fetch_msg(_rgate.chanid());
    }
    /**
     * @return true if EOF has been seen
     */
    bool eof() const {
        return _eof != 0;
    }

    /**
     * Reads at most <count> bytes from the pipe into <buffer>.
     *
     * @param buffer the buffer to read into
     * @param count the number of bytes to read (at most)
     * @return the actual number of read bytes (0 on EOF)
     */
    size_t read(void *buffer, size_t count);

    /**
     * Sends EOF to the writer, i.e. notifies him that you don't want to continue reading. This is
     * done automatically on destruction, but can also be done manually by calling this function.
     */
    void send_eof();

private:
    MemGate _mgate;
    RecvBuf _rbuf;
    RecvGate _rgate;
    size_t _pos;
    size_t _rem;
    size_t _pkglen;
    int _eof;
    GateIStream _is;
};

}
