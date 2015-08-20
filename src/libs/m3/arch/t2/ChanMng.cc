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
#include <m3/tracing/Tracing.h>
#include <m3/ChanMng.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>
#include <m3/Errors.h>

namespace m3 {

void ChanMng::reset() {
    memset(_pos, 0, sizeof(_pos));
    memset(_last, 0, sizeof(_last));
    for(int i = 0; i < CHAN_COUNT; ++i) {
        if(_gates[i])
            _gates[i]->_chanid = Gate::UNBOUND;
        _gates[i] = nullptr;
    }
}

bool ChanMng::fetch_msg(size_t id) {
    // simple way to achieve fairness here. otherwise we might choose the same client all the time
    int end = MAX_CORES;
retry:
    for(int i = _pos[id]; i < end; ++i) {
        volatile Message *msg = reinterpret_cast<volatile Message*>(
            RECV_BUF_LOCAL + DTU::get().recvbuf_offset(i + FIRST_PE_ID, id));
        if(msg->length != 0) {
            LOG(IPC, "Fetched msg @ " << (void*)msg << " over chan " << id);
            if(msg->core < FIRST_PE_ID + MAX_CORES) {
                EVENT_TRACE_MSG_RECV(msg->core, msg->length,
                    ((uint)msg - RECV_BUF_GLOBAL) >> TRACE_ADDR2TAG_SHIFT);
            }
            assert(_last[id] == nullptr);
            _last[id] = const_cast<Message*>(msg);
            _pos[id] = i + 1;
            return true;
        }
    }
    if(_pos[id] != 0) {
        end = _pos[id];
        _pos[id] = 0;
        goto retry;
    }
    return false;
}

void ChanMng::notify(size_t id) {
    Message *msg = message(id);
    RecvGate *gate = reinterpret_cast<RecvGate*>(msg->label);
    LOG(IPC, "Received msg @ " << (void*)msg << " over chan " << id << " -> gate=" << (void*)gate);
#if defined(TRACE_DEBUG)
    uint remote_core = msg->core;
    if((remote_core >= FIRST_PE_ID && remote_core < FIRST_PE_ID + MAX_CORES) ||
        remote_core == MEMORY_CORE) {
        Serial::get() << "Received msg @ " << (void*)msg << " over chan " << id
                      << "  length:" << msg->length << " label: " << msg->label
                      << "  core: " << remote_core << " chanid: " << msg->chanid
                      << "  timestamp: " << Profile::start() << "\n";
    }
#endif
    gate->notify_all();
    ack_message(id);
}

ChanMng::Message *ChanMng::message(size_t id) const {
    assert(_last[id]);
    return _last[id];
}

ChanMng::Message *ChanMng::message_at(size_t id, size_t msgidx) const {
    return reinterpret_cast<Message*>(DTU::get().recvbuf_offset(coreid(), id) + msgidx);
}

size_t ChanMng::get_msgoff(size_t id, RecvGate *rcvgate) const {
    return get_msgoff(id, rcvgate, message(id));
}

size_t ChanMng::get_msgoff(size_t id, UNUSED RecvGate *rcvgate, const ChanMng::Message *msg) const {
    // currently we just return the offset here, because we're implementing reply() in SW.
    return reinterpret_cast<uintptr_t>(msg) - DTU::get().recvbuf_offset(coreid(), id);
}

void ChanMng::ack_message(size_t id) {
    // we might have already acked it by doing a reply
    if(_last[id]) {
        volatile Message *msg = message(id);
        LOG(IPC, "Ack msg @ " << (void*)msg << " over chan " << id);
        msg->length = 0;
        _last[id] = nullptr;
    }
}

}
