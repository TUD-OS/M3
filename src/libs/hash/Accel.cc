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

#include <m3/stream/Standard.h>

#include <hash/Hash.h>

using namespace m3;

namespace hash {

AccelIMem::AccelIMem(bool muxable)
    : _vpe("acc", PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_HASH), nullptr, muxable) {
}

AccelEMem::AccelEMem(bool muxable)
    : _vpe("acc", PEDesc(PEType::COMP_EMEM, PEISA::ACCEL_HASH), "pager", muxable) {
}

uintptr_t AccelIMem::getRBAddr() {
    return _vpe.pe().mem_size() - RECVBUF_SIZE_SPM + DEF_RCVBUF_SIZE + UPCALL_RBUF_SIZE;
}

uintptr_t AccelEMem::getRBAddr() {
    return RECVBUF_SPACE + DEF_RCVBUF_SIZE + UPCALL_RBUF_SIZE;
}

Accel *Accel::create() {
    Accel *acc = new AccelIMem(true);
    if(Errors::last != Errors::NO_ERROR) {
        delete acc;
        acc = new AccelEMem(true);
        if(Errors::last != Errors::NO_ERROR)
            exitmsg("Unable to find accelerator");
    }
    return acc;
}

}
