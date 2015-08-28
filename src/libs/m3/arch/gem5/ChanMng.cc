/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/ChanMng.h>
#include <m3/cap/RecvGate.h>
#include <m3/Syscalls.h>
#include <m3/Errors.h>
#include <m3/Log.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>

namespace m3 {

void ChanMng::notify(size_t id) {
    word_t addr = DTU::get().get_ep(id)->bufferReadPtr;
    Message *msg = message(id);
    LOG(IPC, "Received message over " << id << " @ "
            << fmt(addr, "p") << "+" << fmt(reinterpret_cast<word_t>(msg) - addr, "x"));
    RecvGate *gate = reinterpret_cast<RecvGate*>(msg->label);
    gate->notify_all();
    ack_message(id);
}

}