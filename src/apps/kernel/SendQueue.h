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
        explicit Entry(RecvGate *_rgate, SendGate *_sgate, const void *_msg, size_t _size)
            : SListItem(), rgate(_rgate), sgate(_sgate), msg(_msg), size(_size) {
        }

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

    void send(RecvGate *rgate, SendGate *sgate, const void *msg, size_t size, bool onheap) {
        if(_inflight < _capacity)
            do_send(rgate, sgate, msg, size, onheap);
        else {
            // if it's not already on the heap, put it there
            if(!onheap) {
                void *nmsg = m3::Heap::alloc(size);
                memcpy(nmsg, msg, size);
                msg = nmsg;
            }

            Entry *e = new Entry(rgate, sgate, msg, size);
            _queue.append(e);
        }
    }

    void received_reply() {
        assert(_inflight > 0);
        _inflight--;
        Entry *e = _queue.remove_first();
        if(e) {
            // pending messages have always been copied to the heap
            do_send(e->rgate, e->sgate, e->msg, e->size, true);
            delete e;
        }
    }

private:
    void do_send(RecvGate *rgate, SendGate *sgate, const void *msg, size_t size, bool onheap) {
        sgate->send(msg, size, rgate);
        if(onheap)
            m3::Heap::free(const_cast<void*>(msg));
        _inflight++;
    }

    m3::SList<Entry> _queue;
    int _capacity;
    int _inflight;
};

}
