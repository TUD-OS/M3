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

#include <m3/Common.h>
#include <m3/util/Sync.h>
#include <m3/DTU.h>

#include "../../KDTU.h"

namespace m3 {

void KDTU::wakeup(int core) {
    // wakeup core
    DTU::reg_t cmd = static_cast<DTU::reg_t>(DTU::CmdOpCode::WAKEUP_CORE);
    Sync::compiler_barrier();
    static_assert(offsetof(DTU::CmdRegs, command) == 0, "Command register is not at offset 0");
    write_mem(core, (uintptr_t)DTU::cmd_regs(), &cmd, sizeof(cmd));
}

void KDTU::deprivilege(int core) {
    // unset the privileged flag (writes to other bits are ignored)
    DTU::reg_t status = 0;
    Sync::compiler_barrier();
    static_assert(offsetof(DTU::DtuRegs, status) == 0, "Status register is not at offset 0");
    write_mem(core, (uintptr_t)DTU::dtu_regs(), &status, sizeof(status));
}

void KDTU::invalidate_ep(int core, int ep) {
    DTU::EpRegs e;
    memset(&e, 0, sizeof(e));
    Sync::compiler_barrier();
    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::ep_regs(ep));
    write_mem(core, dst, &e, sizeof(e));
}

void KDTU::invalidate_eps(int core) {
    DTU::EpRegs *eps = new DTU::EpRegs[EP_COUNT];
    size_t total = sizeof(*eps) * EP_COUNT;
    memset(eps, 0, total);
    Sync::compiler_barrier();
    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::ep_regs(DTU::SYSC_EP));
    write_mem(core, dst, eps, total);
    delete[] eps;
}

void KDTU::config_recv(void *e, uintptr_t buf, uint order, uint msgorder, int) {
    DTU::EpRegs *ep = reinterpret_cast<DTU::EpRegs*>(e);
    ep->bufAddr = buf;
    ep->bufSize = static_cast<size_t>(1) << (order - msgorder);
    ep->bufMsgSize = static_cast<size_t>(1) << msgorder;
    ep->bufMsgCnt = 0;
    ep->bufReadPtr = buf;
    ep->bufWritePtr = buf;
}

void KDTU::config_recv_local(int ep, uintptr_t buf, uint order, uint msgorder, int flags) {
    config_recv(DTU::get().ep_regs(ep), buf, order, msgorder, flags);
}

void KDTU::config_recv_remote(int core, int ep, uintptr_t buf, uint order, uint msgorder, bool valid) {
    DTU::EpRegs e;
    memset(&e, 0, sizeof(e));

    if(valid)
        config_recv(&e, buf, order, msgorder, 0);

    // write to PE
    Sync::compiler_barrier();
    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::ep_regs(ep));
    write_mem(core, dst, &e, sizeof(e));
}

void KDTU::config_send(void *e, label_t label, int dstcore, int dstep,
        size_t msgsize, word_t) {
    DTU::EpRegs *ep = reinterpret_cast<DTU::EpRegs*>(e);
    ep->label = label;
    ep->targetCoreId = dstcore;
    ep->targetEpId = dstep;
    // TODO hand out "unlimited" credits atm
    ep->credits = 0xFFFFFFFF;
    ep->maxMsgSize = msgsize;
}

void KDTU::config_send_local(int ep, label_t label, int dstcore, int dstep,
        size_t msgsize, word_t credits) {
    config_send(DTU::get().ep_regs(ep), label, dstcore, dstep, msgsize, credits);
}

void KDTU::config_send_remote(int core, int ep, label_t label, int dstcore, int dstep,
        size_t msgsize, word_t credits) {
    DTU::EpRegs e;
    memset(&e, 0, sizeof(e));
    config_send(&e, label, dstcore, dstep, msgsize, credits);

    // write to PE
    Sync::compiler_barrier();
    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::ep_regs(ep));
    write_mem(core, dst, &e, sizeof(e));
}

void KDTU::config_mem(void *e, int dstcore, uintptr_t addr, size_t size, int perm) {
    DTU::EpRegs *ep = reinterpret_cast<DTU::EpRegs*>(e);
    ep->targetCoreId = dstcore;
    ep->reqRemoteAddr = addr;
    ep->reqRemoteSize = size;
    ep->reqFlags = perm;
}

void KDTU::config_mem_local(int ep, int coreid, uintptr_t addr, size_t size) {
    config_mem(DTU::get().ep_regs(ep), coreid, addr, size, DTU::R | DTU::W);
}

void KDTU::config_mem_remote(int core, int ep, int dstcore, uintptr_t addr, size_t size, int perm) {
    DTU::EpRegs e;
    memset(&e, 0, sizeof(e));
    config_mem(&e, dstcore, addr, size, perm);

    // write to PE
    Sync::compiler_barrier();
    uintptr_t dst = reinterpret_cast<uintptr_t>(DTU::ep_regs(ep));
    write_mem(core, dst, &e, sizeof(e));
}

void KDTU::reply_to(int core, int ep, int, word_t, label_t label, const void *msg, size_t size) {
    config_send_local(_ep, label, core, ep, size + DTU::HEADER_SIZE, size + DTU::HEADER_SIZE);
    DTU::get().send(_ep, msg, size, 0, 0);
}

void KDTU::write_mem(int core, uintptr_t addr, const void *data, size_t size) {
    config_mem_local(_ep, core, addr, size);
    DTU::get().write(_ep, data, size, 0);
}

}
