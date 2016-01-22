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

#include <m3/pipe/PipeReader.h>

namespace m3 {

void PipeReader::send_eof() {
    if(~_eof & Pipe::READ_EOF) {
        // if we have not fetched a message yet, do so now
        if(_pkglen == static_cast<size_t>(-1))
            _is = receive_vmsg(_rgate, _pos, _pkglen);
        DBG_PIPE("[read] replying len=0\n");
        reply_vmsg_on(_is, (size_t)0);
        _eof |= Pipe::READ_EOF;
    }
}

size_t PipeReader::read(void *buffer, size_t count) {
    if(_eof)
        return 0;

    assert((reinterpret_cast<uintptr_t>(buffer) & (DTU_PKG_SIZE - 1)) == 0);
    assert((count & (DTU_PKG_SIZE - 1)) == 0);
    if(_rem == 0) {
        if(_pos > 0) {
            DBG_PIPE("[read] replying len=" << _pkglen << "\n");
            reply_vmsg_on(_is, _pkglen);
            _is.ack();
        }
        _is = receive_vmsg(_rgate, _pos, _pkglen);
        _rem = _pkglen;
    }

    size_t amount = Math::min(count, _rem);
    DBG_PIPE("[read] read from pos=" << _pos << ", len=" << amount << "\n");
    if(amount == 0)
        _eof |= Pipe::WRITE_EOF;
    else {
        _mgate.read_sync(buffer, amount, _pos);
        _pos += amount;
        _rem -= amount;
    }
    return amount;
}

}
