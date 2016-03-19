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

#include <base/Log.h>

#include <m3/session/Pager.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

namespace m3 {

const size_t VPE::BUF_SIZE    = 4096;
VPE VPE::_self INIT_PRIORITY(102);

VPE::VPE(const String &name, const String &core, const char *pager)
        : ObjCap(VIRTPE, VPE::self().alloc_cap()), _mem(MemGate::bind(VPE::self().alloc_cap(), 0)),
          _caps(new BitField<SEL_TOTAL>()), _eps(new BitField<EP_COUNT>()),
          _pager(), _mounts(), _mountlen() {
    init();

    if(pager) {
        // create pager first, to create session and obtain gate cap
        _pager = new Pager(pager);
        // now create VPE, which implicitly obtains the gate cap from us
        Syscalls::get().createvpe(sel(), _mem.sel(), name, core, _pager->gate().sel(), alloc_ep());
        // now delegate our VPE cap to the pager
        _pager->delegate_obj(sel());
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
        Heap::free(_mounts);
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

void VPE::delegate_mounts() {
    assert(!_mounts);
    _mountlen = VFS::serialize_length();
    _mounts = Heap::alloc(_mountlen);
    VFS::serialize(_mounts, _mountlen);
    VFS::delegate(*this, _mounts, _mountlen);
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
