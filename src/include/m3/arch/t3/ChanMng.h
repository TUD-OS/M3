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
#include <assert.h>

namespace m3 {

class Gate;
class RecvGate;

class ChanMng : public ChanMngBase {
    friend class Gate;
    friend class ChanMngBase;

public:
    struct Message : public DTU::Header {
        int send_chanid() const {
            return 0;
        }
        int reply_chanid() const {
            return slot;
        }

        unsigned char data[];
    } PACKED;

    explicit ChanMng() : ChanMngBase() {
    }

    void setlast(size_t, Message *) {
    }

    bool fetch_msg(size_t id);
    void notify(size_t id);
    void ack_message(size_t id);
    Message *message(size_t id) const;
    Message *message_at(size_t id, size_t msgidx) const;
    size_t get_msgoff(size_t id, RecvGate *rcvgate) const;
    size_t get_msgoff(size_t id, RecvGate *rcvgate, const ChanMng::Message *msg) const;
};

inline bool ChanMng::fetch_msg(size_t id) {
    return DTU::get().element_count(id) > 0;
}

inline bool ChanMngBase::uses_header(size_t) const {
    return true;
}

static_assert(sizeof(ChanMng::Message) == DTU::HEADER_SIZE, "Header do not match");

}
