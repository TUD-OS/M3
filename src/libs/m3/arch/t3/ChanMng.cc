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

#include <m3/cap/SendGate.h>
#include <m3/cap/Gate.h>
#include <m3/ChanMng.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>
#include <m3/Errors.h>

namespace m3 {

void ChanMng::reset() {
    for(int i = 0; i < CHAN_COUNT; ++i) {
        if(_gates[i])
            _gates[i]->_chanid = Gate::UNBOUND;
        _gates[i] = nullptr;
    }
}

void ChanMng::notify(size_t id) {
    Message *msg = message(id);
    LOG(IPC, "Received msg @ " << (void*)msg << " over chan " << id);
    RecvGate *gate = reinterpret_cast<RecvGate*>(msg->label);
    gate->notify_all();
    ack_message(id);
}

ChanMng::Message *ChanMng::message(size_t id) const {
    return reinterpret_cast<ChanMng::Message*>(DTU::get().element_ptr(id));
}

ChanMng::Message *ChanMng::message_at(size_t id, UNUSED size_t msgidx) const {
    uintptr_t recvbuf = DTU::get().recvbuf(id);
    size_t msgsize = DTU::get().msgsize(id);
    return reinterpret_cast<ChanMng::Message*>(recvbuf + msgidx * msgsize);
}

size_t ChanMng::get_msgoff(size_t id, RecvGate *rcvgate) const {
    return get_msgoff(id, rcvgate, message(id));
}

size_t ChanMng::get_msgoff(size_t, RecvGate *rcvgate, const ChanMng::Message *msg) const {
    size_t off = (reinterpret_cast<uintptr_t>(msg) - reinterpret_cast<uintptr_t>(rcvgate->buffer()->addr()));
    return off >> rcvgate->buffer()->msgorder();
}

void ChanMng::ack_message(size_t id) {
    if(Log::level & Log::IPC) {
        volatile Message *msg = message(id);
        LOG(IPC, "Ack msg @ " << (void*)msg << " over chan " << id);
    }
    DTU::get().ack_message(id);
}

}
