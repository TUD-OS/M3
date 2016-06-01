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

#include <base/Init.h>
#include <base/Panic.h>

#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileTable.h>
#include <m3/vfs/MountSpace.h>
#include <m3/vfs/SerialFile.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

namespace m3 {

const size_t VPE::BUF_SIZE    = 4096;
INIT_PRIO_VPE VPE VPE::_self;

// don't revoke these. they kernel does so on exit
VPE::VPE()
    : ObjCap(VIRTPE, 0, KEEP_SEL | KEEP_CAP), _mem(MemGate::bind(1)),
      _caps(), _eps(), _pager(), _ms(), _fds() {
    init_state();
    init();
    init_fs();

    if(!_ms)
        _ms = new MountSpace();
    if(!_fds)
        _fds = new FileTable();

    // create stdin, stdout and stderr, if not existing
    if(!_fds->exists(STDIN_FD))
        _fds->set(STDIN_FD, new SerialFile());
    if(!_fds->exists(STDOUT_FD))
        _fds->set(STDOUT_FD, new SerialFile());
    if(!_fds->exists(STDERR_FD))
        _fds->set(STDERR_FD, new SerialFile());
}

VPE::VPE(const String &name, const String &core, const char *pager)
        : ObjCap(VIRTPE, VPE::self().alloc_caps(2)),
          _mem(MemGate::bind(sel() + 1, 0)),
          _caps(new BitField<SEL_TOTAL>()), _eps(new BitField<EP_COUNT>()),
          _pager(), _ms(new MountSpace()), _fds(new FileTable()) {
    init();

    // create pager first, to create session and obtain gate cap
    if(pager)
        _pager = new Pager(pager);
    else if(VPE::self().pager())
        _pager = VPE::self().pager()->create_clone();
    if(Errors::last != Errors::NO_ERROR)
        return;

    if(_pager) {
        // now create VPE, which implicitly obtains the gate cap from us
        Syscalls::get().createvpe(sel(), _mem.sel(), name, core, _pager->gate().sel(), alloc_ep());
        // mark the pfgate cap allocated
        assert(!_caps->is_set(_pager->gate().sel()));
        _caps->set(_pager->gate().sel());
        // now delegate our VPE cap and memory cap to the pager
        _pager->delegate(CapRngDesc(CapRngDesc::OBJ, sel(), 2));
        // and delegate the pager cap to the VPE
        delegate_obj(_pager->sel());
    }
    else
        Syscalls::get().createvpe(sel(), _mem.sel(), name, core, ObjCap::INVALID, 0);
}

VPE::~VPE() {
    if(this != &_self) {
        delete _pager;
        // unarm it first. we can't do that after revoke (which would be triggered by the Gate destructor)
        EPMux::get().remove(&_mem, true);
        // only free that if it's not our own VPE. 1. it doesn't matter in this case and 2. it might
        // be stored not on the heap but somewhere else
        delete _eps;
        delete _caps;
        delete _fds;
        delete _ms;
    }
}

void VPE::init() {
    _caps->set(0);
    _caps->set(1);
    _eps->set(DTU::SYSC_EP);
    _eps->set(DTU::MEM_EP);
    _eps->set(DTU::DEF_RECVEP);
}

capsel_t VPE::alloc_caps(uint count) {
    capsel_t res = _caps->first_clear();

retry:
    if(res + count > SEL_TOTAL)
        PANIC("No more capability selectors");

    // ensure that the first is actually free
    if(_caps->is_set(res)) {
        res++;
        goto retry;
    }

    // check the next count-1 selectors if they are free
    for(uint i = 1; i < count; ++i) {
        if(_caps->is_set(res + i)) {
            res += i + 1;
            goto retry;
        }
    }

    // we've found them
    for(uint i = 0; i < count; ++i)
        _caps->set(res + i);
    return res;
}

size_t VPE::alloc_ep() {
    size_t ep = _eps->first_clear();
    if(ep >= EP_COUNT)
        PANIC("No more free endpoints");
    _eps->set(ep);
    return ep;
}

void VPE::mountspace(const MountSpace &ms) {
    delete _ms;
    _ms = new MountSpace(ms);
}

void VPE::obtain_mountspace() {
    _ms->delegate(*this);
}

void VPE::fds(const FileTable &fds) {
    delete _fds;
    _fds = new FileTable(fds);
}

void VPE::obtain_fds() {
    _fds->delegate(*this);
}

void VPE::delegate(const CapRngDesc &crd) {
    Syscalls::get().exchange(sel(), crd, crd, false);
    for(capsel_t sel = crd.start(); sel != crd.start() + crd.count(); ++sel) {
        if(!VPE::self().is_cap_free(sel))
            _caps->set(sel);
    }
}

void VPE::obtain(const CapRngDesc &crd) {
    obtain(crd, VPE::self().alloc_caps(crd.count()));
}

void VPE::obtain(const CapRngDesc &crd, capsel_t dest) {
    Syscalls::get().exchange(sel(), CapRngDesc(CapRngDesc::OBJ, dest, crd.count()), crd, true);
}

int VPE::wait() {
    int exitcode;
    Syscalls::get().vpectrl(sel(), KIF::Syscall::VCTRL_WAIT, 0, &exitcode);
    return exitcode;
}

}
