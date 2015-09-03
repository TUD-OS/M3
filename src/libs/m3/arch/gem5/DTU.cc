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
    EpRegs *e = ep_regs(ep);
    e->bufAddr = buf;
    e->bufReadPtr = buf;
    e->bufWritePtr = buf;
    e->bufSize = 1UL << (order - msgorder);
    e->bufMsgSize = 1UL << msgorder;
    e->bufMsgCnt = 0;
}

void DTU::send(int ep, const void *msg, size_t size, label_t replylbl, int reply_ep) {
    CmdRegs *c = cmd_regs();
    c->dataAddr = reinterpret_cast<uintptr_t>(msg);
    c->dataSize = size;
    c->replyLabel = replylbl;
    c->replyEpId = reply_ep;
    Sync::compiler_barrier();
    c->command = buildCommand(ep, CmdOpCode::SEND);
}

void DTU::reply(int ep, const void *msg, size_t size, size_t) {
    CmdRegs *c = cmd_regs();
    c->dataAddr = reinterpret_cast<uintptr_t>(msg);
    c->dataSize = size;
    Sync::compiler_barrier();
    c->command = buildCommand(ep, CmdOpCode::REPLY);
}

void DTU::read(int ep, void *msg, size_t size, size_t off) {
    CmdRegs *c = cmd_regs();

    c->dataAddr = reinterpret_cast<uintptr_t>(msg);
    c->dataSize = size;
    c->offset = off;
    Sync::compiler_barrier();
    c->command = buildCommand(ep, CmdOpCode::READ);

    wait_until_ready(ep);
}

void DTU::write(int ep, const void *msg, size_t size, size_t off) {
    CmdRegs *c = cmd_regs();

    c->dataAddr = reinterpret_cast<uintptr_t>(msg);
    c->dataSize = size;
    c->offset = off;
    Sync::compiler_barrier();
    c->command = buildCommand(ep, CmdOpCode::WRITE);

    wait_until_ready(ep);
}

}
