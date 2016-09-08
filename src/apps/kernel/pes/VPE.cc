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

#include <thread/ThreadManager.h>

#include "com/RecvBufs.h"
#include "pes/VPEManager.h"
#include "pes/VPE.h"
#include "Platform.h"
#include "SyscallHandler.h"

namespace kernel {

VPE::VPE(m3::String &&prog, peid_t peid, vpeid_t id, uint flags, epid_t ep, capsel_t pfgate)
    : SListItem(),
      SlabObject<VPE>(),
      _desc(peid, id),
      _flags(flags | F_INIT),
      _refs(0),
      _pid(),
      _state(DEAD),
      _exitcode(),
      _name(std::move(prog)),
      _objcaps(id + 1),
      _mapcaps(id + 1),
      _lastsched(),
      _dtustate(),
      _syscgate(SyscallHandler::get().create_gate(this)),
      _srvgate(SyscallHandler::get().srvepid(), nullptr),
      _as(Platform::pe(pe()).has_virtmem() ? new AddrSpace(ep, pfgate) : nullptr),
      _requires(),
      _exitsubscr(),
      _resumesubscr() {
    _objcaps.set(0, new VPECapability(&_objcaps, 0, this));
    _objcaps.set(1, new MemCapability(&_objcaps, 1, 0, MEMCAP_END, m3::KIF::Perm::RWX, pe(), id, 0));

    // let the VPEManager know about us before we continue with initialization
    VPEManager::get().add(this);

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
        VPEManager::get().remove(this);
}

void VPE::start_app() {
    _flags |= F_HASAPP;

    // when exiting, the program will release one reference
    ref();

    KLOG(VPES, "Starting VPE '" << _name << "' [id=" << id() << "]");

    PEManager::get().start_vpe(this);
}

void VPE::stop_app() {
    if(!has_app())
        return;

    if(_state == RUNNING)
        exit(1);
    else {
        PEManager::get().remove_vpe(this);
        _flags &= ~F_HASAPP;
        unref();
    }
}

void VPE::exit_app(int exitcode) {
    // no update on the PE here, since we don't save the state anyway
    _dtustate.invalidate_eps(m3::DTU::FIRST_FREE_EP);
    rbufs().detach_all(*this, m3::DTU::DEF_RECVEP);

    _exitcode = exitcode;

    _flags &= ~F_HASAPP;

    PEManager::get().remove_vpe(this);

    for(auto it = _exitsubscr.begin(); it != _exitsubscr.end();) {
        auto cur = it++;
        cur->callback(exitcode, &*cur);
        _exitsubscr.unsubscribe(&*cur);
    }

    unref();
}

void VPE::yield() {
    PEManager::get().yield_vpe(this);
}

bool VPE::resume(bool need_app, bool unblock) {
    if(need_app && !has_app())
        return false;

    KLOG(VPES, "Resuming VPE '" << _name << "' [id=" << id() << "]");

    if(unblock)
        PEManager::get().unblock_vpe(this);
    m3::ThreadManager::get().wait_for(this);

    KLOG(VPES, "Resumed VPE '" << _name << "' [id=" << id() << "]");
    return true;
}

void VPE::wakeup() {
    if(_state == RUNNING)
        DTU::get().wakeup(desc());
    else if(has_app())
        PEManager::get().unblock_vpe(this);
}

void VPE::notify_resume() {
    m3::ThreadManager::get().notify(this);

    // notify subscribers
    for(auto it = _resumesubscr.begin(); it != _resumesubscr.end();) {
        auto cur = it++;
        cur->callback(true, &*cur);
        _resumesubscr.unsubscribe(&*cur);
    }
}

void VPE::free_reqs() {
    for(auto it = _requires.begin(); it != _requires.end(); ) {
        auto old = it++;
        delete &*old;
    }
}

void VPE::invalidate_ep(epid_t ep) {
    _dtustate.invalidate(ep);
    update_ep(ep);
}

void VPE::config_snd_ep(epid_t ep, label_t lbl, peid_t pe, vpeid_t vpe, epid_t dstep, size_t msgsize, word_t crd) {
    _dtustate.config_send(ep, lbl, pe, vpe, dstep, msgsize, crd);
    update_ep(ep);
}

void VPE::config_rcv_ep(epid_t ep, uintptr_t buf, uint order, uint msgorder, int flags) {
    _dtustate.config_recv(ep, buf, order, msgorder, flags);
    update_ep(ep);
}

void VPE::config_mem_ep(epid_t ep, peid_t pe, vpeid_t vpe, uintptr_t addr, size_t size, int perm) {
    _dtustate.config_mem(ep, pe, vpe, addr, size, perm);
    update_ep(ep);
}

void VPE::update_ep(epid_t ep) {
    if(state() == VPE::RUNNING)
        DTU::get().write_ep_remote(desc(), ep, _dtustate.get_ep(ep));
}

}
