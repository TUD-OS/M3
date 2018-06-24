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
#include <base/util/Math.h>

#include <thread/ThreadManager.h>

#include "pes/PEManager.h"
#include "pes/VPEManager.h"
#include "pes/VPEGroup.h"
#include "pes/VPE.h"
#include "DTU.h"
#include "Platform.h"
#include "SyscallHandler.h"

namespace kernel {

cycles_t VPE::TIME_SLICE = 6000000;

VPE::VPE(m3::String &&prog, peid_t peid, vpeid_t id, uint flags, epid_t sep, epid_t rep,
         capsel_t sgate, VPEGroup *group)
    : SListItem(),
      SlabObject<VPE>(),
      RefCounted(),
      _desc(peid, id),
      _flags(flags | F_INIT | F_NEEDS_INVAL),
      _pid(),
      _state(DEAD),
      _exitcode(),
      _sysc_ep((flags & F_IDLE) ? SyscallHandler::ep(0) : SyscallHandler::alloc_ep()),
      _group(group),
      _services(),
      _pending_fwds(),
      _name(m3::Util::move(prog)),
      _objcaps(id + 1),
      _mapcaps(id + 1),
      _lastsched(),
      _rbufs_size(),
      _dtustate(),
      _upcsgate(*this, m3::DTU::UPCALL_REP, 0),
      _upcqueue(*this),
      _as(Platform::pe(pe()).has_virtmem() ? new AddrSpace(pe(), id, sep, rep, sgate) : nullptr),
      _headers(),
      _rbufcpy(),
      _requires(),
      _argc(),
      _argv(),
      _epaddr() {
    if(_sysc_ep == EP_COUNT)
        PANIC("Too few slots in syscall receive buffers");
    if(group)
        _group->add(this);

    _objcaps.set(0, new VPECapability(&_objcaps, 0, this));
    _objcaps.set(1, new MGateCapability(&_objcaps, 1, pe(), id, 0, MEMCAP_END, m3::KIF::Perm::RWX));
    for(epid_t ep = m3::DTU::FIRST_FREE_EP; ep < EP_COUNT; ++ep) {
        capsel_t sel = 2 + ep - m3::DTU::FIRST_FREE_EP;
        _objcaps.set(sel, new EPCapability(&_objcaps, sel, id, ep));
    }

    if(!Platform::pe(pe()).has_virtmem())
        _rbufcpy = MainMemory::get().allocate(RECVBUF_SIZE_SPM, PAGE_SIZE);

    // let the VPEManager know about us before we continue with initialization
    VPEManager::get().add(this);

    // we have one reference to ourself
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

    if(_group)
        _group->remove(this);

    free_reqs();

    _objcaps.revoke_all();
    _mapcaps.revoke_all();

    // ensure that there are no syscalls for this VPE anymore
    DTU::get().drop_msgs(syscall_ep(), reinterpret_cast<label_t>(this));
    SyscallHandler::free_ep(syscall_ep());

    if(_rbufcpy)
        MainMemory::get().free(_rbufcpy);

    delete _as;

    VPEManager::get().remove(this);
}

void VPE::flush_cache() {
    if(_flags & F_FLUSHED)
        return;

    KLOG(VPES, "Flushing cache of VPE '" << _name << "' [id=" << id() << "]");

    VPE *cur = PEManager::get().current(pe());
    assert(cur != nullptr);
    DTU::get().flush_cache(cur->desc());
    _flags |= F_FLUSHED;
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
        _flags ^= F_HASAPP;
    }

    if(rem_ref())
        delete this;
}

static int exit_event = 0;

void VPE::wait_for_exit() {
    m3::ThreadManager::get().wait_for(reinterpret_cast<event_t>(&exit_event));
}

void VPE::exit_app(int exitcode) {
    // no update on the PE here, since we don't save the state anyway
    _dtustate.invalidate_eps(m3::DTU::FIRST_FREE_EP);

    // "deactivate" send and receive gates
    for(capsel_t sel = 2; sel < 2 + EP_COUNT - m3::DTU::FIRST_FREE_EP; ++sel) {
        auto epcap = static_cast<EPCapability*>(_objcaps.get(sel, Capability::EP));
        if(epcap == nullptr || epcap->obj->gate == nullptr)
            continue;

        if(epcap->obj->gate->type == Capability::SGATE)
            static_cast<SGateObject*>(epcap->obj->gate)->activated = false;
        else if(epcap->obj->gate->type == Capability::RGATE)
            static_cast<RGateObject*>(epcap->obj->gate)->addr = 0;

        // forget the connection
        epcap->obj->gate->remove_ep(&*epcap->obj);
        epcap->obj->gate = nullptr;
    }

    _exitcode = exitcode;

    _flags ^= F_HASAPP;

    PEManager::get().stop_vpe(this);

    m3::ThreadManager::get().notify(reinterpret_cast<event_t>(&exit_event));
}

void VPE::yield() {
    if(_pending_fwds == 0)
        PEManager::get().yield_vpe(this);
}

bool VPE::migrate(bool fast) {
    // idle VPEs are never migrated
    if(_flags & (VPE::F_IDLE | VPE::F_PINNED))
        return false;

    peid_t old = pe();

    bool changed = PEManager::get().migrate_vpe(this, fast);
    if(changed)
        KLOG(VPES, "Migrated VPE '" << _name << "' [id=" << id() << "] from " << old << " to " << pe());

    return changed;
}

bool VPE::migrate_for(VPE *vpe) {
    if(_flags & (VPE::F_IDLE | VPE::F_PINNED))
        return false;

    peid_t old = pe();

    bool changed = PEManager::get().migrate_for(this, vpe);
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
        m3::ThreadManager::get().wait_for(reinterpret_cast<event_t>(this));

    KLOG(VPES, "Resumed VPE '" << _name << "' [id=" << id() << "]");
    return true;
}

void VPE::wakeup() {
    if(_state == RUNNING)
        DTU::get().inject_irq(desc());
    else if(has_app())
        PEManager::get().unblock_vpe(this, false);
}

void VPE::notify_resume() {
    m3::ThreadManager::get().notify(reinterpret_cast<event_t>(this));
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

bool VPE::invalidate_ep(epid_t ep, bool cmd) {
    KLOG(EPS, "VPE" << id() << ":EP" << ep << " = invalid");

    bool res = true;
    if(cmd) {
        if(is_on_pe())
            res = DTU::get().inval_ep_remote(desc(), ep) == m3::Errors::NONE;
        else
            res = _dtustate.invalidate(ep, true);
    }
    else {
        _dtustate.invalidate(ep, false);
        update_ep(ep);
    }
    return res;
}

bool VPE::can_forward_msg(epid_t ep) {
    if(is_on_pe())
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

m3::Errors::Code VPE::config_rcv_ep(epid_t ep, RGateObject &obj) {
    // it needs to be in the receive buffer space
    const goff_t addr = Platform::def_recvbuf(pe());
    const size_t size = Platform::pe(pe()).has_virtmem() ? RECVBUF_SIZE : RECVBUF_SIZE_SPM;
    // def_recvbuf() == 0 means that we do not validate it
    if(addr && (obj.addr < addr || obj.addr > addr + size || obj.addr + obj.size() > addr + size))
        return m3::Errors::INV_ARGS;
    if(obj.addr < addr + _rbufs_size)
        return m3::Errors::INV_ARGS;

    // no free headers left?
    size_t msgSlots = 1UL << (obj.order - obj.msgorder);
    if(_headers + msgSlots > m3::DTU::HEADER_COUNT)
        return m3::Errors::OUT_OF_MEM;

    obj.header = _headers;
    KLOG(EPS, "VPE" << id() << ":EP" << ep << " = "
        "RGate[addr=#" << m3::fmt(obj.addr, "x")
        << ", order=" << obj.order
        << ", msgorder=" << obj.msgorder
        << ", header=" << obj.header << "]");

    _dtustate.config_recv(ep, obj.addr, obj.order, obj.msgorder, obj.header);
    update_ep(ep);

    // TODO really manage the header space and zero the headers first in case they are reused
    _headers += msgSlots;

    m3::ThreadManager::get().notify(reinterpret_cast<event_t>(&obj));
    return m3::Errors::NONE;
}

m3::Errors::Code VPE::config_snd_ep(epid_t ep, SGateObject &obj) {
    assert(obj.rgate->addr != 0);
    if(obj.activated)
        return m3::Errors::EXISTS;

    peid_t pe = VPEManager::get().peof(obj.rgate->vpe);
    KLOG(EPS, "VPE" << id() << ":EP" << ep << " = "
        "Send[vpe=" << obj.rgate->vpe
        << ", pe=" << pe
        << ", ep=" << obj.rgate->ep
        << ", label=#" << m3::fmt(obj.label, "x")
        << ", msgsize=" << obj.rgate->msgorder << ", crd=" << obj.credits << "]");

    obj.activated = true;
    _dtustate.config_send(ep, obj.label, pe, obj.rgate->vpe,
                          obj.rgate->ep, 1UL << obj.rgate->msgorder, obj.credits);
    update_ep(ep);
    return m3::Errors::NONE;
}

m3::Errors::Code VPE::config_mem_ep(epid_t ep, const MGateObject &obj, goff_t off) {
    if(off >= obj.size || obj.addr + off < off)
        return m3::Errors::INV_ARGS;

    KLOG(EPS, "VPE" << id() << ":EP" << ep << " = "
        "Mem [vpe=" << obj.vpe
        << ", pe=" << obj.pe
        << ", addr=#" << m3::fmt(obj.addr + off, "x")
        << ", size=#" << m3::fmt(obj.size - off, "x")
        << ", perms=#" << m3::fmt(obj.perms, "x") << "]");

    // TODO
    _dtustate.config_mem(ep, obj.pe, obj.vpe, obj.addr + off, obj.size - off, obj.perms);
    update_ep(ep);
    return m3::Errors::NONE;
}

void VPE::update_ep(epid_t ep) {
    if(is_on_pe())
        DTU::get().write_ep_remote(desc(), ep, _dtustate.get_ep(ep));
}

}
