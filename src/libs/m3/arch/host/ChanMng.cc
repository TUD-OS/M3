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
    word_t addr = DTU::get().get_rep(id, DTU::REP_ADDR);
    Message *msg = message(id);
    LOG(IPC, "Received message over " << id << " @ "
            << fmt(addr, "p") << "+" << fmt(reinterpret_cast<word_t>(msg) - addr, "x"));
    RecvGate *gate = reinterpret_cast<RecvGate*>(msg->label);
    gate->notify_all();
    ack_message(id);
}

ChanMng::Message *ChanMng::message(size_t id) const {
    size_t off = DTU::get().get_rep(id, DTU::REP_ROFF);
    word_t addr = DTU::get().get_rep(id, DTU::REP_ADDR);
    size_t ord = DTU::get().get_rep(id, DTU::REP_ORDER);
    return reinterpret_cast<Message*>(addr + (off & ((1UL << ord) - 1)));
}

ChanMng::Message *ChanMng::message_at(size_t id, size_t msgidx) const {
    word_t addr = DTU::get().get_rep(id, DTU::REP_ADDR);
    size_t ord = DTU::get().get_rep(id, DTU::REP_ORDER);
    size_t msgord = DTU::get().get_rep(id, DTU::REP_MSGORDER);
    return reinterpret_cast<Message*>(addr + ((msgidx << msgord) & ((1UL << ord) - 1)));
}

size_t ChanMng::get_msgoff(size_t id, RecvGate *rcvgate) const {
    return get_msgoff(id, rcvgate, message(id));
}

size_t ChanMng::get_msgoff(size_t id, UNUSED RecvGate *rcvgate, const ChanMng::Message *msg) const {
    word_t addr = DTU::get().get_rep(id, DTU::REP_ADDR);
    size_t ord = DTU::get().get_rep(id, DTU::REP_MSGORDER);
    return (reinterpret_cast<word_t>(msg) - addr) >> ord;
}

void ChanMng::ack_message(size_t id) {
    word_t flags = DTU::get().get_rep(id, DTU::REP_FLAGS);
    size_t roff = DTU::get().get_rep(id, DTU::REP_ROFF);
    if(~flags & DTU::FLAG_NO_RINGBUF) {
        size_t ord = DTU::get().get_rep(id, DTU::REP_ORDER);
        size_t msgord = DTU::get().get_rep(id, DTU::REP_MSGORDER);
        roff = (roff + (1UL << msgord)) & ((1UL << (ord + 1)) - 1);
        DTU::get().set_rep(id, DTU::REP_ROFF, roff);
    }
    _msgcnt[id]++;
    LOG(IPC, "Ack message in " << id << " -> roff=#"
            << fmt(roff, "x") << ", cnt=#" << fmt(_msgcnt[id], "x"));
}

void ChanMng::set_msgcnt(size_t id, word_t count) {
    _msgcnt[id] = count;
}

}
