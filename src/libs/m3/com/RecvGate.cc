/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Init.h>

#include <thread/ThreadManager.h>

#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/Syscalls.h>

namespace m3 {

INIT_PRIO_RECVGATE RecvGate RecvGate::_default (RecvGate::create(&RecvBuf::def()));
INIT_PRIO_RECVGATE RecvGate RecvGate::_upcall (RecvGate::create(&RecvBuf::upcall()));

Errors::Code RecvGate::reply(const void *data, size_t len, size_t msgidx) {
    // TODO hack to fix the race-condition on T2. as soon as we've replied to the other core, he
    // might send us another message, which we might miss if we ACK this message after we've got
    // another one. so, ACK it now since the reply marks the end of the handling anyway.
#if defined(__t2__)
    DTU::get().mark_read(epid(), msgidx);
#endif

retry:
    Errors::Code res = DTU::get().reply(epid(), const_cast<void*>(data), len, msgidx);

    if(EXPECT_FALSE(res == Errors::VPE_GONE)) {
        // TODO note that we do not use Gate::reactivate here, because putting them together
        // increases the runtime for the bench-fileread by 10%. not sure why

        void *event = ThreadManager::get().get_wait_event();
        Errors::Code res = Syscalls::get().activatereply(epid(), msgidx, event);

        // if this has been done, go to sleep and wait until the kernel sends us the upcall
        if(res == Errors::UPCALL_REPLY) {
            ThreadManager::get().wait_for(event);
            auto *msg = reinterpret_cast<const KIF::Upcall::Notify*>(
                ThreadManager::get().get_current_msg());
            res = static_cast<Errors::Code>(msg->error);
        }

        if(res != Errors::NO_ERROR)
            return res;
        goto retry;
    }

    return res;
}

Errors::Code RecvGate::wait(SendGate *sgate, DTU::Message **msg) const {
    while(1) {
        *msg = DTU::get().fetch_msg(epid());
        if(*msg)
            return Errors::NO_ERROR;

        if(sgate && !DTU::get().is_valid(sgate->epid()))
            return Errors::EP_INVALID;

        // don't report idles if we wait for a syscall reply
        DTU::get().try_sleep(!sgate || sgate->epid() != m3::DTU::SYSC_EP);
    }
    UNREACHED;
}

}
