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

#include <base/Errors.h>

#include <thread/ThreadManager.h>

#include <m3/com/Gate.h>
#include <m3/Syscalls.h>

namespace m3 {

Errors::Code Gate::async_cmd(Operation op, void *data, size_t datalen, size_t off, size_t size,
        label_t reply_lbl, int reply_ep) {
    // ensure that the DMAUnit is ready. this is required if we want to mix async sends with
    // sync sends.
    wait_until_sent();
    ensure_activated();

retry:
    Errors::Code res;
    switch(op) {
        case SEND:
            res = DTU::get().send(_epid, data, datalen, reply_lbl, reply_ep);
            break;
        case READ:
            res = DTU::get().read(_epid, data, datalen, off, size);
            break;
        case WRITE:
            res = DTU::get().write(_epid, data, datalen, off, size);
            break;
        case CMPXCHG:
            res = DTU::get().cmpxchg(_epid, data, datalen, off, size);
            break;
    }

    if(res == Errors::VPE_GONE) {
        // if we have other threads available, let the kernel reply to us via upcall
        void *event = ThreadManager::get().sleeping_count() > 0 ? this : nullptr;
        res = Syscalls::get().activate(_epid, sel(), sel(), event);

        // if this has been done, go to sleep and wait until the kernel sends us the upcall
        if(res == Errors::UPCALL_REPLY) {
            ThreadManager::get().wait_for(this);
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

}
