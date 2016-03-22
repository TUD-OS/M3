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

#include <m3/pipe/Pipe.h>
#include <m3/pipe/PipeReader.h>

namespace m3 {

PipeReader::State::State(capsel_t caps, size_t rep)
    : _mgate(MemGate::bind(caps)),
      _rbuf(RecvBuf::create(rep, nextlog2<Pipe::MSG_BUF_SIZE>::val, nextlog2<Pipe::MSG_SIZE>::val, 0)),
      _rgate(RecvGate::create(&_rbuf)),
      _pos(), _rem(), _pkglen(-1), _eof(0), _is(_rgate) {
}

PipeReader::PipeReader(capsel_t caps, size_t rep, State *state)
    : _noeof(), _caps(caps), _rep(rep), _state(state) {
}

PipeReader::~PipeReader() {
    send_eof();
    delete _state;
}

void PipeReader::send_eof() {
    if(_noeof)
        return;

    if(!_state)
        _state = new State(_caps, _rep);
    if(~_state->_eof & Pipe::READ_EOF) {
        // if we have not fetched a message yet, do so now
        if(_state->_pkglen == static_cast<size_t>(-1))
            _state->_is = receive_vmsg(_state->_rgate, _state->_pos, _state->_pkglen);
        DBG_PIPE("[read] replying len=0\n");
        reply_vmsg_on(_state->_is, (size_t)0);
        _state->_eof |= Pipe::READ_EOF;
    }
}

ssize_t PipeReader::read(void *buffer, size_t count) {
    if(!_state)
        _state = new State(_caps, _rep);
    if(_state->_eof)
        return 0;

    assert((reinterpret_cast<uintptr_t>(buffer) & (DTU_PKG_SIZE - 1)) == 0);
    assert((count & (DTU_PKG_SIZE - 1)) == 0);
    if(_state->_rem == 0) {
        if(_state->_pos > 0) {
            DBG_PIPE("[read] replying len=" << _state->_pkglen << "\n");
            reply_vmsg_on(_state->_is, _state->_pkglen);
            _state->_is.ack();
        }
        _state->_is = receive_vmsg(_state->_rgate, _state->_pos, _state->_pkglen);
        _state->_rem = _state->_pkglen;
    }

    size_t amount = Math::min(count, _state->_rem);
    DBG_PIPE("[read] read from pos=" << _state->_pos << ", len=" << amount << "\n");
    if(amount == 0)
        _state->_eof |= Pipe::WRITE_EOF;
    else {
        size_t aligned_amount = Math::round_up(amount, DTU_PKG_SIZE);
        _state->_mgate.read_sync(buffer, aligned_amount, _state->_pos);
        _state->_pos += aligned_amount;
        _state->_rem -= amount;
    }
    return amount;
}

size_t PipeReader::serialize_length() {
    return ostreamsize<capsel_t, size_t>();
}

void PipeReader::delegate(VPE &vpe) {
    vpe.delegate(CapRngDesc(CapRngDesc::OBJ, _caps, 1));
}

void PipeReader::serialize(Marshaller &m) {
    // we can't share the reader between two VPEs atm anyway, so don't serialize the current state
    m << _caps << _rep;
}

File *PipeReader::unserialize(Unmarshaller &um) {
    capsel_t caps;
    size_t rep;
    um >> caps >> rep;
    return new PipeReader(caps, rep, nullptr);
}

}
