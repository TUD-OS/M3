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

#include <base/Panic.h>

#include <m3/com/RecvBuf.h>
#include <m3/com/MemGate.h>
#include <m3/session/Pager.h>
#include <m3/Syscalls.h>

namespace m3 {

uint8_t *RecvBuf::allocate(size_t size) {
    // TODO atm, the kernel allocates the complete receive buffer space
    size_t left = RECVBUF_SIZE - (_nextbuf - RECVBUF_SPACE);
    if(size > left)
        PANIC("Not enough receive buffer space for " << size << "b (" << left << "b left)");

    uint8_t *res = reinterpret_cast<uint8_t*>(_nextbuf);
    _nextbuf += size;
    return res;
}

void RecvBuf::free(uint8_t *) {
    // TODO implement me
}

}
