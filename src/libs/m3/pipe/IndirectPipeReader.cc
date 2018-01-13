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

#include <m3/pipe/IndirectPipeReader.h>

#include <limits>

namespace m3 {

ssize_t IndirectPipeReader::read(void *buffer, size_t count) {
    size_t pos;
    Profile::start(0xbbbb);
    Errors::Code res = _pipe->read(&pos, &count);
    Profile::stop(0xbbbb);
    if(res != Errors::NONE)
        return -1;
    if(count == 0)
        return 0;

    Profile::start(0xaaaa);
    _mem.read(buffer, count, pos);
    Profile::stop(0xaaaa);
    return static_cast<ssize_t>(count);
}

Errors::Code IndirectPipeReader::read_next(capsel_t *memgate, size_t *offset, size_t *length) {
    size_t pos;
    size_t count = std::numeric_limits<size_t>::max();
    Errors::Code res = _pipe->read(&pos, &count);
    if(res != Errors::NONE)
        return res;
    *offset = pos;
    *length = count;
    *memgate = _mem.sel();
    return Errors::NONE;
}

void IndirectPipeReader::delegate(VPE &vpe) {
    IndirectPipeFile::delegate(vpe);
    vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _pipe->read_gate().sel(), 1));
    _pipe->attach(true);
}

File *IndirectPipeReader::unserialize(Unmarshaller &um) {
    capsel_t mem, sess, metagate, rdgate, wrgate;
    um >> mem >> sess >> metagate >> rdgate >> wrgate;
    return new IndirectPipeReader(mem, sess, metagate, rdgate, wrgate);
}

}
