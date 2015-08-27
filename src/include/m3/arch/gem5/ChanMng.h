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

#pragma once

#include <m3/util/SList.h>
#include <m3/Log.h>
#include <m3/DTU.h>

namespace m3 {

class Gate;
class RecvGate;
class GateIStream;
class Env;

class ChanMng : public ChanMngBase {
    friend class Env;
    friend class Gate;

public:
    explicit ChanMng() : ChanMngBase(), _refs() {
    }

    struct Message : DTU::Header {
        int send_chanid() const {
            return senderEpId;
        }
        int reply_chanid() const {
            return replyEpId;
        }

        unsigned char data[];
    } PACKED;

    bool fetch_msg(size_t id);
    void notify(size_t id);
    Message *message(size_t id) const;
    Message *message_at(size_t id, size_t msgidx) const;
    size_t get_msgoff(size_t id, RecvGate *rcvgate) const;
    size_t get_msgoff(size_t id, RecvGate *rcvgate, const ChanMng::Message *msg) const;
    void ack_message(size_t id);

private:
    unsigned _refs[CHAN_COUNT];
};

inline bool ChanMng::fetch_msg(size_t id) {
    volatile DTU::Endpoint *ep = DTU::get().get_ep(id);
    return ep->bufferMessageCount > 0;
}

inline bool ChanMngBase::uses_header(size_t) const {
    // TODO unsupported
    return true;
}

inline bool ChanMngBase::uses_ringbuf(size_t) const {
    // TODO unsupported
    return true;
}

inline void ChanMngBase::set_msgcnt(size_t, word_t) {
}

inline ChanMng::Message *ChanMng::message(size_t id) const {
    return reinterpret_cast<Message*>(DTU::get().get_ep(id)->bufferReadPtr);
}

inline ChanMng::Message *ChanMng::message_at(size_t, size_t) const {
    // TODO unsupported
    return nullptr;
}

inline size_t ChanMng::get_msgoff(size_t, RecvGate *) const {
    return 0;
}

inline size_t ChanMng::get_msgoff(size_t, RecvGate *, const ChanMng::Message *) const {
    // TODO unsupported
    return 0;
}

inline void ChanMng::ack_message(size_t id) {
    DTU::get().execCommand(id, DTU::Command::INC_READ_PTR);
    LOG(IPC, "Ack message in " << id);
}

}
