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

#include <m3/Common.h>
#include <m3/pipe/Pipe.h>

namespace m3 {

/**
 * Writes into a previously constructed pipe.
 */
class PipeWriter {
public:
    /**
     * Constructs a pipe-writer for given pipe.
     *
     * @param p the pipe
     */
    explicit PipeWriter(const Pipe &p) : PipeWriter(p.caps(), p.size()) {
    }
    explicit PipeWriter(capsel_t caps, size_t size)
        : _mgate(MemGate::bind(caps)),
          _rbuf(RecvBuf::create(VPE::self().alloc_ep(),
            nextlog2<Pipe::MSG_BUF_SIZE>::val, nextlog2<Pipe::MSG_SIZE>::val, 0)),
          _rgate(RecvGate::create(&_rbuf)), _sgate(SendGate::bind(caps + 1, &_rgate)),
          _size(size), _free(_size), _rdpos(), _wrpos(),
          _capacity(Pipe::MSG_BUF_SIZE / Pipe::MSG_SIZE), _eof(0) {
    }
    /**
     * Sends EOF and waits for all outstanding replies
     */
    ~PipeWriter() {
        send_eof();
        read_replies();
        VPE::self().free_ep(_rbuf.epid());
    }

    /**
     * @return true if EOF has been seen
     */
    bool eof() const {
        return _eof != 0;
    }
    /**
     * Sends EOF to the reader, i.e. notifies him that you are done sending data. This is done
     * automatically on destruction, but can also be done manually by calling this function.
     */
    void send_eof() {
        if(!_eof) {
            write(nullptr, 0);
            _eof |= Pipe::WRITE_EOF;
        }
    }

    /**
     * Writes <count> bytes at <buffer> into the pipe.
     *
     * @param buffer the data to write
     * @param count the number of bytes to write
     * @return the number of written bytes (0 if it failed)
     */
    size_t write(const void *buffer, size_t count);

private:
    size_t find_spot(size_t *len);
    void read_replies();

    MemGate _mgate;
    RecvBuf _rbuf;
    RecvGate _rgate;
    SendGate _sgate;
    size_t _size;
    size_t _free;
    size_t _rdpos;
    size_t _wrpos;
    int _capacity;
    int _eof;
};

}
