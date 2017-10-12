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

#include <base/util/Util.h>

#include <m3/pipe/IndirectPipe.h>
#include <m3/pipe/IndirectPipeReader.h>
#include <m3/pipe/IndirectPipeWriter.h>
#include <m3/vfs/FileTable.h>
#include <m3/VPE.h>

namespace m3 {

IndirectPipe::IndirectPipe(MemGate &mem, size_t memsize)
    : _mem(mem), _pipe("pipe", memsize),
      _rdfd(VPE::self().fds()->alloc(new IndirectPipeReader(_mem.sel(), &_pipe))),
      _wrfd(VPE::self().fds()->alloc(new IndirectPipeWriter(_mem.sel(), &_pipe))) {
}

IndirectPipe::~IndirectPipe() {
    close_reader();
    close_writer();
}

void IndirectPipe::close_reader() {
    delete VPE::self().fds()->free(_rdfd);
}

void IndirectPipe::close_writer() {
    delete VPE::self().fds()->free(_wrfd);
}

size_t IndirectPipeFile::serialize_length() {
    return ostreamsize<capsel_t, capsel_t, capsel_t, capsel_t, capsel_t>();
}

void IndirectPipeFile::serialize(Marshaller &m) {
    m << _mem.sel() << _pipe->sel() << _pipe->meta_gate().sel();
    m << _pipe->read_gate().sel() << _pipe->write_gate().sel();
}

void IndirectPipeFile::delegate(VPE &vpe) {
    vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _mem.sel(), 1));
    vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _pipe->sel(), 1));
    vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _pipe->meta_gate().sel(), 1));
}

}
