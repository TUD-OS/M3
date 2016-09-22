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

namespace kernel {

void SendQueue::send(VPE *vpe, RecvGate *rgate, SendGate *sgate, const void *msg, size_t size, bool onheap) {
    KLOG(SQUEUE, "SendQueue: trying to send message to VPE " << vpe->id());

    if(vpe->state() == VPE::RUNNING && _inflight < _capacity)
        do_send(vpe, rgate, sgate, msg, size, onheap);
    else {
        // call this again from the workloop to be sure that we can switch the thread
        if(vpe->state() != VPE::RUNNING && _inflight == 0)
            Timeouts::get().wait_for(0, std::bind(&SendQueue::send_pending, this));

        // if it's not already on the heap, put it there
        if(!onheap) {
            void *nmsg = m3::Heap::alloc(size);
            memcpy(nmsg, msg, size);
            msg = nmsg;
        }

        KLOG(SQUEUE, "SendQueue: queuing message for VPE " << vpe->id());

        Entry *e = new Entry(vpe, rgate, sgate, msg, size);
        _queue.append(e);
    }
}

void SendQueue::send_pending() {
    if(_queue.length() == 0)
        return;

    Entry *e = _queue.remove_first();

    KLOG(SQUEUE, "SendQueue: found pending message for VPE " << e->vpe->id());

    // ensure that the VPE is running
    if(e->vpe->state() != VPE::RUNNING) {
        // if it died, just drop the pending message
        if(!e->vpe->resume()) {
            delete e;
            return;
        }
    }

    // pending messages have always been copied to the heap
    do_send(e->vpe, e->rgate, e->sgate, e->msg, e->size, true);
    delete e;
}

void SendQueue::received_reply(VPE &vpe) {
    KLOG(SQUEUE, "SendQueue: received reply from VPE " << vpe.id());

    assert(_inflight > 0);
    _inflight--;
    send_pending();
}

void SendQueue::do_send(VPE *vpe, RecvGate *rgate, SendGate *sgate, const void *msg, size_t size, bool onheap) {
    KLOG(SQUEUE, "SendQueue: sending message to VPE " << vpe->id());

    sgate->send(msg, size, rgate);
    if(onheap)
        m3::Heap::free(const_cast<void*>(msg));
    _inflight++;
}

}
