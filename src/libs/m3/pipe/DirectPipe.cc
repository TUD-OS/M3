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

#include <m3/pipe/DirectPipe.h>
#include <m3/pipe/DirectPipeReader.h>
#include <m3/pipe/DirectPipeWriter.h>
#include <m3/vfs/FileTable.h>

namespace m3 {

DirectPipe::DirectPipe(VPE &rd, VPE &wr, size_t size)
    : _rd(rd), _wr(wr), _recvep(rd.alloc_ep()), _size(size),
      _mem(MemGate::create_global(size, MemGate::RW, VPE::self().alloc_caps(2))),
      _sgate(SendGate::create_for(rd, _recvep, 0, CREDITS, nullptr, _mem.sel() + 1)),
      _rdfd(), _wrfd() {
    assert(Math::is_aligned(size, DTU_PKG_SIZE));

    DirectPipeReader::State *rstate = &rd == &VPE::self() ? new DirectPipeReader::State(caps(), _recvep) : nullptr;
    _rdfd = VPE::self().fds()->alloc(new DirectPipeReader(caps(), _recvep, rstate));

    DirectPipeWriter::State *wstate = &wr == &VPE::self() ? new DirectPipeWriter::State(caps(), _size) : nullptr;
    _wrfd = VPE::self().fds()->alloc(new DirectPipeWriter(caps(), _size, wstate));
}

DirectPipe::~DirectPipe() {
    close_writer();
    close_reader();
    _rd.free_ep(_recvep);
}

void DirectPipe::close_reader() {
    DirectPipeReader *rd = static_cast<DirectPipeReader*>(VPE::self().fds()->free(_rdfd));
    if(rd) {
        // don't send EOF, if we are not reading
        if(&_rd != &VPE::self())
            rd->_noeof = true;
        delete rd;
    }
}

void DirectPipe::close_writer() {
    DirectPipeWriter *wr = static_cast<DirectPipeWriter*>(VPE::self().fds()->free(_wrfd));
    if(wr) {
        // don't send EOF, if we are not writing
        if(&_wr != &VPE::self())
            wr->_noeof = true;
        delete wr;
    }
}

}
