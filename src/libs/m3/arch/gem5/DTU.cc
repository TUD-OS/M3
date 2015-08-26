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

#include <m3/util/Sync.h>
#include <m3/DTU.h>

namespace m3 {

DTU DTU::inst INIT_PRIORITY(106);

void DTU::set_receiving(int ep, uintptr_t buf, uint order, uint msgorder, int) {
    Endpoint *e = get_ep(ep);
    e->mode = RECEIVE_MESSAGE;
    e->bufferAddr = buf;
    e->bufferReadPtr = buf;
    e->bufferWritePtr = buf;
    e->bufferSize = 1UL << order;
    e->maxMessageSize = 1UL << msgorder;
    e->bufferMessageCount = 0;
}

void DTU::send(int ep, const void *msg, size_t size, label_t replylbl, int reply_ep) {
    Endpoint *e = get_ep(ep);
    assert(e->mode == TRANSMIT_MESSAGE);
    e->messageAddr = reinterpret_cast<uintptr_t>(msg);
    e->messageSize = size;
    e->replyLabel = replylbl;
    e->replyEpId = reply_ep;
    Sync::compiler_barrier();
    execCommand(ep, Command::START_OPERATION);
}

void DTU::reply(int ep, const void *msg, size_t size, size_t) {
    Endpoint *e = get_ep(ep);
    assert(e->mode == RECEIVE_MESSAGE);
    e->messageAddr = reinterpret_cast<uintptr_t>(msg);
    e->messageSize = size;
    Sync::compiler_barrier();
    execCommand(ep, Command::START_OPERATION);
}

void DTU::read(int ep, void *msg, size_t size, size_t off) {
    Endpoint *e = get_ep(ep);
    assert(e->mode == READ_MEMORY || e->mode == WRITE_MEMORY);
    e->mode = READ_MEMORY;

    // TODO workaround since we don't have an offset register yet
    e->requestRemoteAddr += off;

    e->requestLocalAddr = reinterpret_cast<uintptr_t>(msg);
    e->requestSize = size;
    Sync::compiler_barrier();
    execCommand(ep, Command::START_OPERATION);

    wait_until_ready(ep);

    e->requestRemoteAddr -= off;
}

void DTU::write(int ep, const void *msg, size_t size, size_t off) {
    Endpoint *e = get_ep(ep);
    assert(e->mode == READ_MEMORY || e->mode == WRITE_MEMORY);
    e->mode = WRITE_MEMORY;

    // TODO workaround since we don't have an offset register yet
    e->requestRemoteAddr += off;

    e->requestLocalAddr = reinterpret_cast<uintptr_t>(msg);
    e->requestSize = size;
    Sync::compiler_barrier();
    execCommand(ep, Command::START_OPERATION);

    wait_until_ready(ep);

    e->requestRemoteAddr -= off;
}

}
