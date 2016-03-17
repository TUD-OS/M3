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

#include <m3/EnvBackend.h>
#include <m3/cap/VPE.h>
#include <m3/RecvBuf.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>

namespace m3 {

void EnvBackend::exit(int code) {
    Syscalls::get().exit(code);
}

void EnvBackend::attach_recvbuf(RecvBuf *rb) {
#if defined(__t3__)
    // required for t3 because one can't write to these registers externally
    DTU::get().configure_recv(rb->epid(), reinterpret_cast<word_t>(rb->addr()), rb->order(),
        rb->msgorder(), rb->flags());
#endif

    if(rb->epid() > DTU::DEF_RECVEP) {
        Errors::Code res = Syscalls::get().attachrb(VPE::self().sel(), rb->epid(),
            reinterpret_cast<word_t>(rb->addr()), rb->order(),
            rb->msgorder(), rb->flags());
        if(res != Errors::NO_ERROR)
            PANIC("Attaching receive buffer to " << rb->epid() << " failed: " << Errors::to_string(Errors::last));
    }
}

void EnvBackend::detach_recvbuf(RecvBuf *rb) {
    if(rb->epid() > DTU::DEF_RECVEP && rb->epid() != RecvBuf::UNBOUND)
        Syscalls::get().detachrb(VPE::self().sel(), rb->epid());
}

void EnvBackend::switch_ep(size_t victim, capsel_t oldcap, capsel_t newcap) {
    if(Syscalls::get().activate(victim, oldcap, newcap) != Errors::NO_ERROR) {
        // if we wanted to deactivate a cap, we can ignore the failure
        if(newcap != ObjCap::INVALID)
            PANIC("Unable to arm SEP " << victim << ": " << Errors::last);
    }
}

}
