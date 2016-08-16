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

#include <base/util/Profile.h>

#include <m3/com/GateStream.h>
#include <m3/pipe/DirectPipe.h>
#include <m3/pipe/DirectPipeWriter.h>

namespace m3 {

DirectPipeWriter::State::State(capsel_t caps, size_t size)
    : _mgate(MemGate::bind(caps)),
      _rbuf(RecvBuf::create(VPE::self().alloc_ep(),
        nextlog2<DirectPipe::MSG_BUF_SIZE>::val, nextlog2<DirectPipe::MSG_SIZE>::val, 0)),
      _rgate(RecvGate::create(&_rbuf)), _sgate(SendGate::bind(caps + 1, &_rgate)),
      _size(size), _free(_size), _rdpos(), _wrpos(),
      _capacity(DirectPipe::MSG_BUF_SIZE / DirectPipe::MSG_SIZE), _eof() {
}

DirectPipeWriter::State::~State() {
    VPE::self().free_ep(_rbuf.epid());
}

size_t DirectPipeWriter::State::find_spot(size_t *len) {
    if(_free == 0)
        return -1;
    if(_wrpos >= _rdpos) {
        if(_wrpos < _size) {
            *len = Math::min(*len, _size - _wrpos);
            return _wrpos;
        }
        if(_rdpos > 0) {
            *len = Math::min(*len, _rdpos);
            return 0;
        }
        return -1;
    }
    if(_rdpos - _wrpos > 0) {
        *len = Math::min(*len, _rdpos - _wrpos);
        return _wrpos;
    }
    return -1;
}

void DirectPipeWriter::State::read_replies() {
    // read all expected responses
    if(~_eof & DirectPipe::READ_EOF) {
        size_t len = 1;
        int cap = DirectPipe::MSG_BUF_SIZE / DirectPipe::MSG_SIZE;
        while(len && _capacity < cap) {
            receive_vmsg(_rgate, len);
            DBG_PIPE("[shutdown] got len=" << len << "\n");
            _capacity++;
        }
    }
}

DirectPipeWriter::DirectPipeWriter(capsel_t caps, size_t size, State *state)
    : File(), _caps(caps), _size(size), _state(state), _noeof() {
}

DirectPipeWriter::~DirectPipeWriter() {
    send_eof();
    if(_state)
        _state->read_replies();
    delete _state;
}

void DirectPipeWriter::send_eof() {
    if(_noeof)
        return;

    if(!_state)
        _state = new State(_caps, _size);
    if(!_state->_eof) {
        write(nullptr, 0);
        _state->_eof |= DirectPipe::WRITE_EOF;
    }
}

ssize_t DirectPipeWriter::write(const void *buffer, size_t count) {
    if(!_state)
        _state = new State(_caps, _size);
    if(_state->_eof)
        return 0;

    assert((reinterpret_cast<uintptr_t>(buffer) & (DTU_PKG_SIZE - 1)) == 0);

    ssize_t rem = count;
    const char *buf = reinterpret_cast<const char*>(buffer);
    do {
        size_t aligned_amount = Math::round_up(rem, static_cast<ssize_t>(DTU_PKG_SIZE));
        size_t off = _state->find_spot(&aligned_amount);
        if(_state->_capacity == 0 || off == static_cast<size_t>(-1)) {
            size_t len;
            receive_vmsg(_state->_rgate, len);
            DBG_PIPE("[write] got len=" << len << "\n");
            len = Math::round_up(len, DTU_PKG_SIZE);
            _state->_rdpos = (_state->_rdpos + len) % _state->_size;
            _state->_free += len;
            _state->_capacity++;
            if(len == 0) {
                _state->_eof |= DirectPipe::READ_EOF;
                return 0;
            }
            if(_state->_capacity == 0 || off == static_cast<size_t>(-1)) {
                off = _state->find_spot(&aligned_amount);
                if(off == static_cast<size_t>(-1))
                    return 0;
            }
        }

        size_t amount = Math::min(static_cast<ssize_t>(aligned_amount), rem);
        DBG_PIPE("[write] send pos=" << off << ", len=" << amount << "\n");

        if(aligned_amount) {
            Profile::start(0xaaaa);
            _state->_mgate.write_sync(buf, aligned_amount, off);
            Profile::stop(0xaaaa);
            _state->_wrpos = (off + aligned_amount) % _size;
        }
        _state->_free -= aligned_amount;
        _state->_capacity--;
        send_vmsg(_state->_sgate, off, amount);
        rem -= aligned_amount;
        buf += aligned_amount;
    }
    while(rem > 0);
    return buf - reinterpret_cast<const char*>(buffer);
}

size_t DirectPipeWriter::serialize_length() {
    return ostreamsize<capsel_t, size_t>();
}

void DirectPipeWriter::delegate(VPE &vpe) {
    vpe.delegate(CapRngDesc(CapRngDesc::OBJ, _caps, 2));
}

void DirectPipeWriter::serialize(Marshaller &m) {
    // we can't share the writer between two VPEs atm anyway, so don't serialize the current state
    m << _caps << _size;
}

File *DirectPipeWriter::unserialize(Unmarshaller &um) {
    capsel_t caps;
    size_t size;
    um >> caps >> size;
    return new DirectPipeWriter(caps, size, new State(caps, size));
}

}
