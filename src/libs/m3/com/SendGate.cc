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

#include <m3/com/SendGate.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

#include <thread/ThreadManager.h>

#include <assert.h>

namespace m3 {

SendGate SendGate::create(word_t credits, RecvGate *rcvgate, capsel_t sel) {
    rcvgate = rcvgate == nullptr ? &RecvGate::def() : rcvgate;
    return create_for(rcvgate->buffer(), rcvgate->label(), credits, rcvgate, sel);
}

SendGate SendGate::create_for(RecvBuf *rbuf, label_t label, word_t credits, RecvGate *rcvgate, capsel_t sel) {
    uint flags = 0;
    rcvgate = rcvgate == nullptr ? &RecvGate::def() : rcvgate;
    if(sel == INVALID)
        sel = VPE::self().alloc_cap();
    else
        flags |= KEEP_SEL;
    // when we create a send gate for one of our endpoints, it has to be bound to an endpoint (and stay
    // there) because somebody else wants to send us messages to this (or better: to the attached
    // receive gate)
    SendGate gate(sel, flags, rcvgate);
    Syscalls::get().creategate(rbuf->sel(), gate.sel(), label, credits);
    return gate;
}

Errors::Code SendGate::send(const void *data, size_t len) {
    ensure_activated();

    Errors::Code res = DTU::get().send(epid(), data, len, _rcvgate->label(), _rcvgate->epid());
    if(EXPECT_FALSE(res == Errors::VPE_GONE)) {
        void *event = ThreadManager::get().get_wait_event();
        res = Syscalls::get().forwardmsg(sel(), data, len, _rcvgate->epid(), _rcvgate->label(), event);

        // if this has been done, go to sleep and wait until the kernel sends us the upcall
        if(res == Errors::UPCALL_REPLY) {
            ThreadManager::get().wait_for(event);
            auto *msg = reinterpret_cast<const KIF::Upcall::Notify*>(
                ThreadManager::get().get_current_msg());
            res = static_cast<Errors::Code>(msg->error);
        }
    }

    return res;
}

}
