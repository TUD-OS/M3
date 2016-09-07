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

#include "Gate.h"

namespace kernel {

class SendQueue {
    struct Entry : public m3::SListItem {
        explicit Entry(VPE *_vpe, RecvGate *_rgate, SendGate *_sgate, const void *_msg, size_t _size)
            : SListItem(), vpe(_vpe), rgate(_rgate), sgate(_sgate), msg(_msg), size(_size) {
        }

        VPE *vpe;
        RecvGate *rgate;
        SendGate *sgate;
        const void *msg;
        size_t size;
    };

public:
    explicit SendQueue(int capacity) : _queue(), _capacity(capacity), _inflight(0) {
    }

    int inflight() const {
        return _inflight;
    }
    int pending() const {
        return _queue.length();
    }

    void send(VPE *vpe, RecvGate *rgate, SendGate *sgate, const void *msg, size_t size, bool onheap);
    void send_pending();
    void received_reply();

private:
    void do_send(RecvGate *rgate, SendGate *sgate, const void *msg, size_t size, bool onheap);

    m3::SList<Entry> _queue;
    int _capacity;
    int _inflight;
};

}
