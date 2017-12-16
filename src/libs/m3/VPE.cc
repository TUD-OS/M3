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
#include <m3/vfs/FileTable.h>
#include <m3/vfs/MountTable.h>
#include <m3/vfs/SerialFile.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

namespace m3 {

const size_t VPE::BUF_SIZE    = 4096;
INIT_PRIO_VPE VPE VPE::_self;

// don't revoke these. they kernel does so on exit
VPE::VPE()
    : ObjCap(VIRTPE, 0, KEEP_CAP), _pe(env()->pedesc),
      _mem(MemGate::bind(1)), _next_sel(SEL_START), _eps(), _pager(), _rbufcur(), _rbufend(),
      _ms(), _fds(), _exec() {
    static_assert(EP_COUNT < 64, "64 endpoints are the maximum due to the 64-bit bitmask");
    init_state();
    init_fs();

    if(!_ms)
        _ms = new MountTable();
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

VPE::VPE(const String &name, const PEDesc &pe, const char *pager, bool tmuxable)
        : ObjCap(VIRTPE, VPE::self().alloc_caps(2)),
          _pe(pe), _mem(MemGate::bind(sel() + 1, 0)),
          _next_sel(SEL_START), _eps(),
          _pager(), _rbufcur(), _rbufend(),
          _ms(new MountTable()), _fds(new FileTable()), _exec(), _tmuxable(tmuxable) {
    // create pager first, to create session and obtain gate cap
    if(_pe.has_virtmem()) {
        if(pager)
            _pager = new Pager(*this, pager);
        else if(VPE::self().pager())
            _pager = VPE::self().pager()->create_clone(*this);
        if(Errors::last != Errors::NONE)
            return;
    }

    if(_pager) {
        // now create VPE, which implicitly obtains the gate cap from us
        Syscalls::get().createvpe(sel(), _mem.sel(), _pager->gate().sel(),
            name, _pe, alloc_ep(), _pager->rep(), tmuxable);
        // mark the send gate cap allocated
        _next_sel = Math::max(_pager->gate().sel() + 1, _next_sel);
        // now delegate our VPE cap and memory cap to the pager
        _pager->delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel(), 2));
        // and delegate the pager cap to the VPE
        delegate_obj(_pager->sel());
    }
    else {
        Syscalls::get().createvpe(sel(), _mem.sel(), ObjCap::INVALID, name, _pe,
            0, 0, tmuxable);
    }
}

VPE::~VPE() {
    if(this != &_self) {
        stop();
        delete _pager;
        // unarm it first. we can't do that after revoke (which would be triggered by the Gate destructor)
        EPMux::get().remove(&_mem, true);
        // only free that if it's not our own VPE. 1. it doesn't matter in this case and 2. it might
        // be stored not on the heap but somewhere else
        delete _fds;
        delete _ms;
        delete _exec;
    }
}

epid_t VPE::alloc_ep() {
    for(epid_t ep = DTU::FIRST_FREE_EP; ep < EP_COUNT; ++ep) {
        if(is_ep_free(ep)) {
            _eps |= static_cast<uint64_t>(1) << ep;
            return ep;
        }
    }

    PANIC("No more free endpoints");
}

void VPE::mounts(const MountTable &ms) {
    delete _ms;
    _ms = new MountTable(ms);
}

void VPE::obtain_mounts() {
    _ms->delegate(*this);
}

void VPE::fds(const FileTable &fds) {
    delete _fds;
    _fds = new FileTable(fds);
}

void VPE::obtain_fds() {
    _fds->delegate(*this);
}

Errors::Code VPE::delegate(const KIF::CapRngDesc &crd, capsel_t dest) {
    Errors::Code res = Syscalls::get().exchange(sel(), crd, dest, false);
    if(res == Errors::NONE)
        _next_sel = Math::max(_next_sel, dest + crd.count());
    return res;
}

Errors::Code VPE::obtain(const KIF::CapRngDesc &crd) {
    return obtain(crd, VPE::self().alloc_caps(crd.count()));
}

Errors::Code VPE::obtain(const KIF::CapRngDesc &crd, capsel_t dest) {
    KIF::CapRngDesc own(crd.type(), dest, crd.count());
    return Syscalls::get().exchange(sel(), own, crd.start(), true);
}

Errors::Code VPE::revoke(const KIF::CapRngDesc &crd, bool delonly) {
    return Syscalls::get().revoke(sel(), crd, !delonly);
}

Errors::Code VPE::start() {
    xfer_t arg = 0;
    return Syscalls::get().vpectrl(sel(), KIF::Syscall::VCTRL_START, &arg);
}

Errors::Code VPE::stop() {
    xfer_t arg = 0;
    return Syscalls::get().vpectrl(sel(), KIF::Syscall::VCTRL_STOP, &arg);
}

int VPE::wait() {
    xfer_t exitcode;
    Syscalls::get().vpectrl(sel(), KIF::Syscall::VCTRL_WAIT, &exitcode);
    return static_cast<int>(exitcode);
}

}
