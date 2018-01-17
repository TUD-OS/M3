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

#include <accel/stream/StreamAccel.h>

using namespace m3;

namespace accel {

const size_t StreamAccelVPE::BUF_MAX_SIZE  = 0x8000;
const size_t StreamAccelVPE::BUF_ADDR      = 0x6000;

StreamAccelVPE::StreamAccelVPE(const m3::PEDesc &desc, const char *pager, bool muxable)
    : VPE("acc", desc, pager, muxable) {
}

StreamIAccelVPE::StreamIAccelVPE(m3::PEISA isa, bool muxable)
    : StreamAccelVPE(PEDesc(PEType::COMP_IMEM, isa), nullptr, muxable) {
}

StreamEAccelVPE::StreamEAccelVPE(m3::PEISA isa, bool muxable)
    : StreamAccelVPE(PEDesc(PEType::COMP_EMEM, isa), "pager", muxable) {
}

uintptr_t StreamIAccelVPE::getRBAddr() {
    return pe().mem_size() - RB_SIZE;
}

uintptr_t StreamEAccelVPE::getRBAddr() {
    return RECVBUF_SPACE + SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE;
}

StreamAccelVPE *StreamAccelVPE::create(m3::PEISA isa) {
    StreamAccelVPE *acc = new StreamIAccelVPE(isa, true);
    if(Errors::last != Errors::NONE) {
        delete acc;
        acc = new StreamEAccelVPE(isa, true);
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to find accelerator");
    }
    return acc;
}

}
