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
    explicit ChanMng() : ChanMngBase() {
    }

    struct Message {
        int send_chanid() const {
            return snd_chanid;
        }
        int reply_chanid() const {
            return rpl_chanid;
        }

        size_t length;
        unsigned char opcode;
        label_t label;
        struct {
            unsigned has_replycap : 1,
                     core : 15,
                     rpl_chanid : 8,
                     snd_chanid : 8;
        } PACKED;
        label_t replylabel;
        word_t : sizeof(word_t) * 8;
        unsigned char data[];
    } PACKED;

    bool fetch_msg(size_t id);
    void notify(size_t id);
    Message *message(size_t id) const;
    Message *message_at(size_t id, size_t msgidx) const;
    size_t get_msgoff(size_t id, RecvGate *rcvgate) const;
    size_t get_msgoff(size_t id, RecvGate *rcvgate, const ChanMng::Message *msg) const;
    void ack_message(size_t id);
};

inline bool ChanMng::fetch_msg(size_t id) {
    return DTU::get().get_ep(id, DTU::EP_BUF_MSGCNT) > 0;
}

inline bool ChanMngBase::uses_header(size_t id) const {
    return ~DTU::get().get_ep(id, DTU::EP_BUF_FLAGS) & DTU::FLAG_NO_HEADER;
}

static_assert(sizeof(ChanMng::Message) == DTU::HEADER_SIZE, "Header do not match");

}
