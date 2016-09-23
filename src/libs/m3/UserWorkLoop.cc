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

#include <m3/com/GateStream.h>
#include <m3/com/RecvGate.h>
#include <m3/UserWorkLoop.h>

#include <thread/ThreadManager.h>

namespace m3 {

void UserWorkLoop::multithreaded(uint count) {
    RecvGate::upcall().subscribe([] (GateIStream &is, Subscriber<GateIStream&> *) {
        auto &msg = reinterpret_cast<const KIF::Upcall::Notify&>(is.message().data);
        assert(msg.opcode == KIF::Upcall::NOTIFY);

        void *event = reinterpret_cast<void*>(msg.event);
        ThreadManager::get().notify(event, &msg, sizeof(msg));

        KIF::DefaultReply reply;
        reply.error = Errors::NO_ERROR;
        reply_msg(is, &reply, sizeof(reply));
    });

    for(uint i = 0; i < count; ++i)
        new Thread(thread_startup, nullptr);
}

}
