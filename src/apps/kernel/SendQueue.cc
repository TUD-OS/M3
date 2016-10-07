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

#include <base/log/Kernel.h>

#include "pes/Timeouts.h"
#include "pes/VPE.h"
#include "SendQueue.h"
#include "SyscallHandler.h"

namespace kernel {

void *SendQueue::get_event(uint64_t id) {
    return reinterpret_cast<void*>(static_cast<uint64_t>(1) << 63 | id);
}

void *SendQueue::send(SendGate *sgate, const void *msg, size_t size, bool onheap) {
    KLOG(SQUEUE, "SendQueue[" << _vpe.id() << "]: trying to send message");

    if(_vpe.state() == VPE::RUNNING && _inflight == 0)
        return do_send(sgate, _next_id++, msg, size, onheap);

    // call this again from the workloop to be sure that we can switch the thread
    if(_vpe.state() != VPE::RUNNING && _inflight == 0)
        Timeouts::get().wait_for(0, std::bind(&SendQueue::send_pending, this));

    // if it's not already on the heap, put it there
    if(!onheap) {
        void *nmsg = m3::Heap::alloc(size);
        memcpy(nmsg, msg, size);
        msg = nmsg;
    }

    KLOG(SQUEUE, "SendQueue[" << _vpe.id() << "]: queuing message");

    Entry *e = new Entry(_next_id++, sgate, msg, size);
    _queue.append(e);
    return get_event(e->id);
}

void SendQueue::send_pending() {
    if(_queue.length() == 0)
        return;

    Entry *e = _queue.remove_first();

    KLOG(SQUEUE, "SendQueue[" << _vpe.id() << "]: found pending message");

    // ensure that the VPE is running
    while(_vpe.state() != VPE::RUNNING) {
        // if it died, just drop the pending message
        if(!_vpe.resume()) {
            delete e;
            return;
        }

        // it might happen that there is another message in flight now
        if(_inflight != 0) {
            KLOG(SQUEUE, "SendQueue[" << _vpe.id() << "]: queuing message");
            _queue.append(e);
            return;
        }
    }

    // pending messages have always been copied to the heap
    do_send(e->sgate, e->id, e->msg, e->size, true);
    delete e;
}

void SendQueue::received_reply(epid_t epid, const m3::DTU::Message *msg) {
    KLOG(SQUEUE, "SendQueue[" << _vpe.id() << "]: received reply");

    assert(_inflight > 0);
    _inflight--;

    m3::ThreadManager::get().notify(_cur_event, msg, msg->length + sizeof(m3::DTU::Message::Header));

    // now that we've copied the message, we can mark it read
    m3::DTU::get().mark_read(epid, reinterpret_cast<size_t>(msg));

    send_pending();
}

void *SendQueue::do_send(SendGate *sgate, uint64_t id, const void *msg, size_t size, bool onheap) {
    KLOG(SQUEUE, "SendQueue[" << _vpe.id() << "]: sending message");

    _cur_event = get_event(id);
    _inflight++;

    sgate->send(msg, size, SyscallHandler::get().srvepid(), reinterpret_cast<label_t>(this));
    if(onheap)
        m3::Heap::free(const_cast<void*>(msg));
    return _cur_event;
}

}
