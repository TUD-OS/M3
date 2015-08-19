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

#if defined(__host__)

#include <m3/SendQueue.h>
#include <m3/DTU.h>
#include <m3/Log.h>

namespace m3 {

SendQueue SendQueue::_inst INIT_PRIORITY(105);

void SendQueue::work() {
    if(_queue.length() > 0) {
        if(DTU::get().is_ready()) {
            SendItem *it = _queue.remove_first();
            LOG(IPC, "Removing " << it << " from queue");
            delete it;
            if(_queue.length() > 0) {
                SendItem &first = *_queue.begin();
                first.gate.send_async(first.data, first.len);
                LOG(IPC, "Sending " << &first << " from queue");
            }
        }
    }
}

void SendQueue::send_async(SendItem &it) {
    it.gate.send_async(it.data, it.len);
    LOG(IPC, "Sending " << &it << " from queue");
}

}

#endif
