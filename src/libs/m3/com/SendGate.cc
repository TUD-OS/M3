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

SendGate SendGate::create(RecvBuf *rbuf, label_t label, word_t credits, capsel_t sel) {
    rbuf = rbuf == nullptr ? &RecvBuf::def() : rbuf;
    return create_for(rbuf, label, credits, nullptr, sel);
}

SendGate SendGate::create_for(RecvBuf *rbuf, label_t label, word_t credits, RecvBuf *replybuf, capsel_t sel) {
    uint flags = 0;
    replybuf = replybuf == nullptr ? &RecvBuf::def() : replybuf;
    if(sel == INVALID)
        sel = VPE::self().alloc_cap();
    else
        flags |= KEEP_SEL;
    SendGate gate(sel, flags, replybuf);
    Syscalls::get().creategate(rbuf->sel(), gate.sel(), label, credits);
    return gate;
}

Errors::Code SendGate::send(const void *data, size_t len) {
    ensure_activated();

    Errors::Code res = DTU::get().send(ep(), data, len, 0, _replybuf->ep());
    if(EXPECT_FALSE(res == Errors::VPE_GONE)) {
        void *event = ThreadManager::get().get_wait_event();
        res = Syscalls::get().forwardmsg(sel(), data, len, _replybuf->ep(), 0, event);

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
