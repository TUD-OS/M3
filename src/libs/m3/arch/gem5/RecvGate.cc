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

#include <m3/com/RecvGate.h>
#include <m3/com/MemGate.h>
#include <m3/session/Pager.h>
#include <m3/Syscalls.h>

namespace m3 {

void *RecvGate::allocate(VPE &vpe, epid_t, size_t size) {
    // use values in env for VPE::self to work around initialization order problems
    uint64_t *cur = vpe.sel() == 0 ? &env()->rbufcur : &vpe._rbufcur;
    uint64_t *end = vpe.sel() == 0 ? &env()->rbufend : &vpe._rbufend;

    // TODO this assumes that we don't VPE::run between SPM and non-SPM PEs
    if(*end == 0) {
        PEDesc desc = vpe.sel() == 0 ? env()->pedesc : vpe.pe();
        if(desc.has_virtmem()) {
            *cur = RECVBUF_SPACE;
            *cur += SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE + DEF_RBUF_SIZE;
            *end = RECVBUF_SPACE + RECVBUF_SIZE;
        }
        else {
            *cur = desc.mem_size() - RECVBUF_SIZE_SPM;
            *cur += SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE + DEF_RBUF_SIZE;
            *end = desc.mem_size();
        }
    }

    // TODO atm, the kernel allocates the complete receive buffer space
    size_t left = *end - *cur;
    if(size > left)
        PANIC("Not enough receive buffer space for " << size << "b (" << left << "b left)");

    uint8_t *res = reinterpret_cast<uint8_t*>(*cur);
    *cur += size;
    return res;
}

void RecvGate::free(void *) {
    // TODO implement me
}

}
