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

VPE::VPE(m3::String &&prog, peid_t peid, vpeid_t id, uint flags, epid_t ep, capsel_t pfgate)
    : _desc(peid, id),
      _flags(flags),
      _refs(0),
      _pid(),
      _state(DEAD),
      _exitcode(),
      _entry(),
      _name(std::move(prog)),
      _objcaps(id + 1),
      _mapcaps(id + 1),
      _dtustate(),
      _eps(),
      _syscgate(SyscallHandler::get().create_gate(this)),
      _srvgate(SyscallHandler::get().srvepid(), nullptr),
      _as(Platform::pe(pe()).has_virtmem() ? new AddrSpace(ep, pfgate) : nullptr),
      _requires(),
      _exitsubscr(),
      _resumesubscr() {
    _objcaps.set(0, new VPECapability(&_objcaps, 0, this));
    _objcaps.set(1, new MemCapability(&_objcaps, 1, 0, MEMCAP_END, m3::KIF::Perm::RWX, pe(), id, 0));

    // let the VPEManager know about us before we continue with initialization
    VPEManager::get()._vpes[id] = this;

    if(~_flags & F_IDLE)
        init();

    KLOG(VPES, "Created VPE '" << _name << "' [id=" << id << ", pe=" << pe() << "]");
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
    PEManager::get().add_vpe(this);
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
    // no update on the PE here, since we don't save the state anyway
    _dtustate.invalidate_eps(m3::DTU::FIRST_FREE_EP);
    rbufs().detach_all(*this, m3::DTU::DEF_RECVEP);

    _exitcode = exitcode;

    PEManager::get().remove_vpe(this);

    for(auto it = _exitsubscr.begin(); it != _exitsubscr.end();) {
        auto cur = it++;
        cur->callback(exitcode, &*cur);
        _exitsubscr.unsubscribe(&*cur);
    }
}

void VPE::invalidate_ep(epid_t ep) {
    _dtustate.invalidate(ep);
    if(state() == VPE::RUNNING)
        DTU::get().write_ep_remote(desc(), ep, _dtustate.get_ep(ep));
}

void VPE::config_snd_ep(epid_t ep, label_t lbl, peid_t pe, vpeid_t vpe, epid_t dstep, size_t msgsize, word_t crd) {
    _dtustate.config_send(ep, lbl, pe, vpe, dstep, msgsize, crd);
    if(state() == VPE::RUNNING)
        DTU::get().write_ep_remote(desc(), ep, _dtustate.get_ep(ep));
}

void VPE::config_rcv_ep(epid_t ep, uintptr_t buf, uint order, uint msgorder, int flags) {
    _dtustate.config_recv(ep, buf, order, msgorder, flags);
    if(state() == VPE::RUNNING)
        DTU::get().write_ep_remote(desc(), ep, _dtustate.get_ep(ep));
}

void VPE::config_mem_ep(epid_t ep, peid_t pe, vpeid_t vpe, uintptr_t addr, size_t size, int perm) {
    _dtustate.config_mem(ep, pe, vpe, addr, size, perm);
    if(state() == VPE::RUNNING)
        DTU::get().write_ep_remote(desc(), ep, _dtustate.get_ep(ep));
}

}
