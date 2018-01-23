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

#include <base/util/Time.h>

#include <m3/pipe/IndirectPipeWriter.h>

namespace m3 {

ssize_t IndirectPipeWriter::write(const void *buffer, size_t count) {
    size_t pos = 0;
    Time::start(0xbbbb);
    Errors::Code res = _pipe->write(&pos, count, _lastwrite);
    Time::stop(0xbbbb);
    if(res != Errors::NONE)
        return -1;

    _lastwrite = count;
    Time::start(0xaaaa);
    _mem.write(buffer, count, pos);
    Time::stop(0xaaaa);
    return static_cast<ssize_t>(count);
}

Errors::Code IndirectPipeWriter::begin_write(capsel_t *memgate, size_t *offset, size_t *length) {
    size_t pos;
    *length = 256 * 1024;    // TODO be smarter about that
    Errors::Code res = _pipe->write(&pos, *length, _lastwrite);
    if(res != Errors::NONE)
        return res;
    *offset = pos;
    *memgate = _mem.sel();
    _lastwrite = 0;
    return Errors::NONE;
}

void IndirectPipeWriter::commit_write(size_t length) {
    _lastwrite += length;
}

void IndirectPipeWriter::delegate(VPE &vpe) {
    IndirectPipeFile::delegate(vpe);
    vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _pipe->write_gate().sel(), 1));
    _pipe->attach(false);
}

File *IndirectPipeWriter::unserialize(Unmarshaller &um) {
    capsel_t mem, sess, metagate, rdgate, wrgate;
    um >> mem >> sess >> metagate >> rdgate >> wrgate;
    return new IndirectPipeWriter(mem, sess, metagate, rdgate, wrgate);
}

}
