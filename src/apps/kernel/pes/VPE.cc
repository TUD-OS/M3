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
      _pid(),
      _state(DEAD),
      _exitcode(),
      _pending_fwds(),
      _name(m3::Util::move(prog)),
      _objcaps(id + 1),
      _mapcaps(id + 1),
      _lastsched(),
      _epcaps(),
      _dtustate(),
      _upcsgate(*this, m3::DTU::UPCALL_REP, 0),
      _upcqueue(*this),
      _as(Platform::pe(pe()).has_virtmem() ? new AddrSpace(ep, pfgate) : nullptr),
      _requires(),
      _argc(),
      _argv() {
    _objcaps.set(0, new VPECapability(&_objcaps, 0, this));
    _objcaps.set(1, new MGateCapability(&_objcaps, 1, pe(), id, 0, MEMCAP_END, m3::KIF::Perm::RWX));

    // let the VPEManager know about us before we continue with initialization
    VPEManager::get().add(this);

    // we have one reference to itself
    rem_ref();

    if(~_flags & F_IDLE)
        init();

    KLOG(VPES, "Created VPE '" << _name << "' [id=" << id << ", pe=" << pe() << "]");
    for(auto &r : _requires)
        KLOG(VPES, "  requires: '" << r.name << "'");
}

VPE::~VPE() {
    KLOG(VPES, "Deleting VPE '" << _name << "' [id=" << id() << "]");

    _state = DEAD;

    free_reqs();

    _objcaps.revoke_all();
    _mapcaps.revoke_all();

    // ensure that there are no syscalls for this VPE anymore
    DTU::get().drop_msgs(SyscallHandler::get().ep(), reinterpret_cast<label_t>(this));

    delete _as;

    VPEManager::get().remove(this);
}

void VPE::make_daemon() {
    _flags |= F_DAEMON;
    VPEManager::get()._daemons++;
}

void VPE::start_app(int pid) {
    if(has_app())
        return;

    _pid = pid;
    _flags |= F_HASAPP;

    // when exiting, the program will release one reference
    add_ref();

    KLOG(VPES, "Starting VPE '" << _name << "' [id=" << id() << "]");

    PEManager::get().start_vpe(this);
}

void VPE::stop_app(int exitcode, bool self) {
    if(!has_app())
        return;

    if(self)
        exit_app(exitcode);
    else if(_state == RUNNING)
        exit_app(1);
    else {
        PEManager::get().stop_vpe(this);
        _flags &= ~F_HASAPP;
    }

    if(rem_ref())
        delete this;
}

void VPE::wait_for_exit() {
    m3::ThreadManager::get().wait_for(&_exitcode);
}

void VPE::exit_app(int exitcode) {
    // no update on the PE here, since we don't save the state anyway
    _dtustate.invalidate_eps(m3::DTU::FIRST_FREE_EP);

    _exitcode = exitcode;

    _flags &= ~F_HASAPP;

    PEManager::get().stop_vpe(this);

    m3::ThreadManager::get().notify(&_exitcode);
}

void VPE::yield() {
    if(_pending_fwds == 0)
        PEManager::get().yield_vpe(this);
}

bool VPE::migrate() {
    // idle VPEs are never migrated
    if(_flags & VPE::F_IDLE)
        return false;

    peid_t old = pe();

    bool changed = PEManager::get().migrate_vpe(this);
    if(changed)
        KLOG(VPES, "Migrated VPE '" << _name << "' [id=" << id() << "] from " << old << " to " << pe());

    return changed;
}

bool VPE::resume(bool need_app, bool unblock) {
    if(need_app && !has_app())
        return false;

    KLOG(VPES, "Resuming VPE '" << _name << "' (unblock=" << unblock << ") [id=" << id() << "]");

    bool wait = true;
    if(unblock)
        wait = !PEManager::get().unblock_vpe(this, false);
    if(wait)
        m3::ThreadManager::get().wait_for(this);

    KLOG(VPES, "Resumed VPE '" << _name << "' [id=" << id() << "]");
    return true;
}

void VPE::wakeup() {
    if(_state == RUNNING)
        DTU::get().wakeup(desc());
    else if(has_app())
        PEManager::get().unblock_vpe(this, false);
}

void VPE::notify_resume() {
    m3::ThreadManager::get().notify(this);
}

void VPE::free_reqs() {
    for(auto it = _requires.begin(); it != _requires.end(); ) {
        auto old = it++;
        delete &*old;
    }
}

void VPE::upcall_notify(m3::Errors::Code res, word_t event) {
    m3::KIF::Upcall::Notify msg;
    msg.opcode = m3::KIF::Upcall::NOTIFY;
    msg.error = res;
    msg.event = event;
    KLOG(UPCALLS, "Sending upcall NOTIFY (error=" << res << ", event="
        << (void*)event << ") to VPE " << id());
    upcall(&msg, sizeof(msg), false);
}

void VPE::invalidate_ep(epid_t ep) {
    KLOG(EPS, "VPE" << id() << ": EP" << ep << " = invalid");

    _dtustate.invalidate(ep);
    update_ep(ep);
}

bool VPE::can_forward_msg(epid_t ep) {
    if(state() == VPE::RUNNING)
        _dtustate.read_ep(desc(), ep);
    return _dtustate.can_forward_msg(ep);
}

void VPE::forward_msg(epid_t ep, peid_t pe, vpeid_t vpe) {
    KLOG(EPS, "VPE" << id() << ":EP" << ep << " forward message");

    _dtustate.forward_msg(ep, pe, vpe);
    update_ep(ep);
}

void VPE::forward_mem(epid_t ep, peid_t pe) {
    KLOG(EPS, "VPE" << id() << ":EP" << ep << " forward mem");

    _dtustate.forward_mem(ep, pe);
    update_ep(ep);
}

m3::Errors::Code VPE::config_rcv_ep(epid_t ep, const RGateObject &obj) {
    if(obj.addr < Platform::rw_barrier(pe()))
        return m3::Errors::INV_ARGS;

    for(size_t i = 0; i < ARRAY_SIZE(_epcaps); ++i) {
        if(m3::DTU::FIRST_FREE_EP + i == ep || !_epcaps[i] || _epcaps[i]->type != Capability::RGATE)
            continue;

        RGateCapability *cap = static_cast<RGateCapability*>(_epcaps[i]);
        if(m3::Math::overlap(cap->obj->addr, cap->obj->addr + cap->obj->size(),
            obj.addr, obj.addr + cap->obj->size()))
            return m3::Errors::INV_ARGS;
    }

    KLOG(EPS, "VPE" << id() << ":EP" << ep << " = "
        "RGate[addr=#" << m3::fmt(obj.addr, "x")
        << ", order=" << obj.order
        << ", msgorder=" << obj.msgorder << "]");

    _dtustate.config_recv(ep, obj.addr, obj.order, obj.msgorder);
    update_ep(ep);

    m3::ThreadManager::get().notify(&obj);
    return m3::Errors::NO_ERROR;
}

void VPE::config_snd_ep(epid_t ep, const SGateObject &obj) {
    assert(obj.rgate->addr != 0);
    peid_t pe = VPEManager::get().peof(obj.rgate->vpe);
    KLOG(EPS, "VPE" << id() << ":EP" << ep << " = "
        "Send[vpe=" << obj.rgate->vpe
        << ", pe=" << pe
        << ", ep=" << obj.rgate->ep
        << ", label=#" << m3::fmt(obj.label, "x")
        << ", msgsize=" << obj.rgate->msgorder << ", crd=" << obj.credits << "]");

    _dtustate.config_send(ep, obj.label, pe, obj.rgate->vpe,
        obj.rgate->ep, 1UL << obj.rgate->msgorder, obj.credits);
    update_ep(ep);
}

void VPE::config_mem_ep(epid_t ep, const MGateObject &obj) {
    KLOG(EPS, "VPE" << id() << ":EP" << ep << " = "
        "Mem [vpe=" << obj.vpe
        << ", pe=" << obj.pe
        << ", addr=#" << m3::fmt(obj.addr, "x")
        << ", size=#" << m3::fmt(obj.size, "x")
        << ", perms=#" << m3::fmt(obj.perms, "x") << "]");

    // TODO
    _dtustate.config_mem(ep, obj.pe, obj.vpe, obj.addr, obj.size, obj.perms);
    update_ep(ep);
}

void VPE::update_ep(epid_t ep) {
    if(state() == VPE::RUNNING)
        DTU::get().write_ep_remote(desc(), ep, _dtustate.get_ep(ep));
}

}
