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

#include <base/Common.h>
#include <base/log/Kernel.h>

#include "com/RecvBufs.h"
#include "pes/VPEManager.h"
#include "pes/VPE.h"
#include "Platform.h"
#include "SyscallHandler.h"

namespace kernel {

VPE::VPE(m3::String &&prog, int coreid, vpeid_t id, uint flags, int ep, capsel_t pfgate, bool tmuxable)
    : _desc(coreid, id),
      _flags(flags),
      _refs(0),
      _pid(),
      _state(DEAD),
      _exitcode(),
      _tmuxable(tmuxable),
      _entry(),
      _name(std::move(prog)),
      _objcaps(id + 1),
      _mapcaps(id + 1),
      _dtu_state(),
      _eps(),
      _syscgate(SyscallHandler::get().create_gate(this)),
      _srvgate(SyscallHandler::get().srvepid(), nullptr),
      _as(Platform::pe(core()).has_virtmem() ? new AddrSpace(ep, pfgate) : nullptr),
      _requires(),
      _exitsubscr(),
      _resumesubscr() {
    _objcaps.set(0, new VPECapability(&_objcaps, 0, this));
    _objcaps.set(1, new MemCapability(&_objcaps, 1, 0, MEMCAP_END, m3::KIF::Perm::RWX, core(), id, 0));

    // let the VPEManager know about us before we continue with initialization
    VPEManager::get()._vpes[id] = this;

    if(~_flags & F_IDLE)
        init();

    KLOG(VPES, "Created VPE '" << _name << "' [id=" << id << ", pe=" << core() << "]");
    for(auto &r : _requires)
        KLOG(VPES, "  requires: '" << r.name << "'");
}

void VPE::make_daemon() {
    _flags |= F_DAEMON;
    VPEManager::get()._daemons++;
}

void VPE::unref() {
    // 1 because we always have a VPE-cap for ourself (not revokeable)
    if(--_refs == 1)
        VPEManager::get().remove(id());
}

void VPE::set_ready() {
    assert(_state == DEAD);
    assert(!(_flags & (F_INIT | F_START)));

    _flags |= F_INIT;
    PEManager::get().add_vpe(core(), this);
}

void VPE::start() {
    assert(_state == RUNNING);

    _flags |= F_START;

    // when exiting, the program will release one reference
    ref();

    KLOG(VPES, "Starting VPE '" << _name << "' [id=" << id() << "]");

    VPEManager::get().start(id());
}

void VPE::stop() {
    if(_state == RUNNING) {
        exit(1);
        unref();
    }
}

void VPE::exit(int exitcode) {
    invalidate_eps(m3::DTU::FIRST_FREE_EP);
    rbufs().detach_all(*this, m3::DTU::DEF_RECVEP);

    _exitcode = exitcode;

    PEManager::get().remove_vpe(this);

    for(auto it = _exitsubscr.begin(); it != _exitsubscr.end();) {
        auto cur = it++;
        cur->callback(exitcode, &*cur);
        _exitsubscr.unsubscribe(&*cur);
    }
}

m3::DTU::reg_t *VPE::ep_regs(int ep) {
    m3::DTU::reg_t *regs = reinterpret_cast<m3::DTU::reg_t*>(dtu_state());
    return regs + m3::DTU::DTU_REGS + m3::DTU::CMD_REGS + m3::DTU::EP_REGS * ep;
}

void VPE::wakeup() {
    assert(state() == VPE::RUNNING);
    PEManager::get().start_vpe(this);
}

void VPE::invalidate_ep(int ep) {
    if(state() != VPE::RUNNING)
        memset(ep_regs(ep), 0, sizeof(m3::DTU::reg_t) * m3::DTU::EP_REGS);
    else
        DTU::get().invalidate_ep(desc(), ep);
}

void VPE::invalidate_eps(int first) {
    if(state() != VPE::RUNNING)
        memset(ep_regs(0), 0, sizeof(m3::DTU::reg_t) * m3::DTU::EP_REGS * EP_COUNT);
    else
        DTU::get().invalidate_eps(desc(), first);
}

void VPE::config_snd_ep(int ep, label_t label, int dstcore, int dstvpe, int dstep, size_t msgsize,
        word_t credits) {
    if(state() != VPE::RUNNING)
        DTU::get().config_send(ep_regs(ep), label, dstcore, dstvpe, dstep, msgsize, credits);
    else
        DTU::get().config_send_remote(desc(), ep, label, dstcore, dstvpe, dstep, msgsize, credits);
}

void VPE::config_rcv_ep(int ep, uintptr_t buf, uint order, uint msgorder, int flags, bool valid) {
    if(state() != VPE::RUNNING) {
        if(valid)
            DTU::get().config_recv(ep_regs(ep), buf, order, msgorder, flags);
        else
            memset(ep_regs(ep), 0, sizeof(m3::DTU::reg_t) * m3::DTU::EP_REGS);
    }
    else
        DTU::get().config_recv_remote(desc(), ep, buf, order, msgorder, flags, valid);
}

void VPE::config_mem_ep(int ep, int dstcore, int dstvpe, uintptr_t addr, size_t size, int perm) {
    if(state() != VPE::RUNNING)
        DTU::get().config_mem(ep_regs(ep), dstcore, dstvpe, addr, size, perm);
    else
        DTU::get().config_mem_remote(desc(), ep, dstcore, dstvpe, addr, size, perm);
}

}
