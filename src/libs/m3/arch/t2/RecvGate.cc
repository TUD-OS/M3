/*
 * Copyright (C) 2016-2017, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/com/RecvGate.h>

namespace m3 {

void *RecvGate::allocate(VPE &, epid_t ep, size_t size) {
    uintptr_t offset = DTU::get().recvbuf_offset(env()->pe, ep);
    return reinterpret_cast<void*>(RECV_BUF_LOCAL + offset);
}

void RecvGate::free(void *) {
}

}
