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
#include <m3/pipe/PipeWriter.h>
#include <m3/vfs/FileTable.h>

namespace m3 {

Pipe::Pipe(VPE &rd, VPE &wr, size_t size)
    : _rd(rd), _wr(wr), _recvep(rd.alloc_ep()), _size(size),
      _mem(MemGate::create_global(size, MemGate::RW, VPE::self().alloc_caps(2))),
      _sgate(SendGate::create_for(rd, _recvep, 0, CREDITS, nullptr, _mem.sel() + 1)),
      _rdfd(), _wrfd() {
    assert(Math::is_aligned(size, DTU_PKG_SIZE));

    PipeReader::State *rstate = &rd == &VPE::self() ? new PipeReader::State(caps(), _recvep) : nullptr;
    _rdfd = VPE::self().fds()->alloc(new PipeReader(caps(), _recvep, rstate));

    PipeWriter::State *wstate = &wr == &VPE::self() ? new PipeWriter::State(caps(), _size) : nullptr;
    _wrfd = VPE::self().fds()->alloc(new PipeWriter(caps(), _size, wstate));
}

Pipe::~Pipe() {
    close_writer();
    close_reader();
    _rd.free_ep(_recvep);
}

void Pipe::close_reader() {
    PipeReader *rd = static_cast<PipeReader*>(VPE::self().fds()->free(_rdfd));
    if(rd) {
        // don't send EOF, if we are not reading
        if(&_rd != &VPE::self())
            rd->_noeof = true;
        delete rd;
    }
}

void Pipe::close_writer() {
    PipeWriter *wr = static_cast<PipeWriter*>(VPE::self().fds()->free(_wrfd));
    if(wr) {
        // don't send EOF, if we are not writing
        if(&_wr != &VPE::self())
            wr->_noeof = true;
        delete wr;
    }
}

}
