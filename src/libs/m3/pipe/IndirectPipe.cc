/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/FileTable.h>
#include <m3/VPE.h>

namespace m3 {

IndirectPipe::IndirectPipe(MemGate &mem, size_t memsize, const char *service, int flags)
    : _pipe(service, mem, memsize),
      _rdfd(VPE::self().fds()->alloc(_pipe.create_channel(true, flags))),
      _wrfd(VPE::self().fds()->alloc(_pipe.create_channel(false, flags))) {
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

}
