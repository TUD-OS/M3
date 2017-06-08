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
#include <m3/Syscalls.h>

#include <accel/hash/HashAccel.h>

using namespace m3;

namespace accel {

const size_t HashAccel::BUF_SIZE    = 8192;
const size_t HashAccel::BUF_ADDR    = 0x4000;
const size_t HashAccel::STATE_SIZE  = 1024;
const size_t HashAccel::STATE_ADDR  = BUF_ADDR - STATE_SIZE;

HashIAccel::HashIAccel(bool muxable)
    : _vpe("acc", PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_SHA), nullptr, muxable),
      _spm(MemGate::create_global(BUF_SIZE + STATE_SIZE, MemGate::RW)) {
    Syscalls::get().activate(_vpe.sel(), _spm.sel(), MEM_EP, 0);
}

HashEAccel::HashEAccel(bool muxable)
    : _vpe("acc", PEDesc(PEType::COMP_DTUVM, PEISA::ACCEL_SHA), "pager", muxable) {
}

uintptr_t HashIAccel::getRBAddr() {
    return _vpe.pe().mem_size() - RB_SIZE;
}

uintptr_t HashEAccel::getRBAddr() {
    return RECVBUF_SPACE + SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE;
}

HashAccel *HashAccel::create() {
    HashAccel *acc = new HashIAccel(true);
    if(Errors::last != Errors::NONE) {
        delete acc;
        acc = new HashEAccel(true);
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to find accelerator");
    }
    return acc;
}

}
