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

#pragma once

#include <base/Common.h>
#include <base/col/SList.h>
#include <base/Heap.h>
#include <base/DTU.h>

#include "Gate.h"

namespace kernel {

class SendQueue {
    struct Entry : public m3::SListItem {
        explicit Entry(uint64_t _id, SendGate *_sgate, const void *_msg, size_t _size)
            : SListItem(), id(_id), sgate(_sgate), msg(_msg), size(_size) {
        }

        uint64_t id;
        SendGate *sgate;
        const void *msg;
        size_t size;
    };

public:
    explicit SendQueue(VPE &vpe)
        : _vpe(vpe), _queue(), _next_id(), _cur_event(), _inflight(0) {
    }
    ~SendQueue();

    VPE &vpe() const {
        return _vpe;
    }
    int inflight() const {
        return _inflight;
    }
    int pending() const {
        return _queue.length();
    }

    void *send(SendGate *sgate, const void *msg, size_t size, bool onheap);
    void received_reply(epid_t ep, const m3::DTU::Message *msg);

private:
    void send_pending();
    void *get_event(uint64_t id);
    void *do_send(SendGate *sgate, uint64_t id, const void *msg, size_t size, bool onheap);

    VPE &_vpe;
    m3::SList<Entry> _queue;
    uint64_t _next_id;
    void *_cur_event;
    int _inflight;
};

}
