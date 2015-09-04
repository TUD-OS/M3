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

#include <m3/cap/VPE.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>

namespace m3 {

const size_t VPE::BUF_SIZE    = 4096;
VPE VPE::_self INIT_PRIORITY(102);

VPE::VPE(const String &name, const String &core)
        : Cap(VIRTPE, VPE::self().alloc_cap()), _mem(MemGate::bind(VPE::self().alloc_cap(), 0)),
          _caps(new BitField<SEL_TOTAL>()), _chans(new BitField<CHAN_COUNT>()),
          _mounts(), _mountlen() {
    init();
    Syscalls::get().createvpe(sel(), _mem.sel(), name, core);
}

VPE::~VPE() {
    if(this != &_self) {
        // unarm it first. we can't do that after revoke (which would be triggered by the Gate destructor)
        EPMux::get().remove(&_mem, true);
        // only free that if it's not our own VPE. 1. it doesn't matter in this case and 2. it might
        // be stored not on the heap but somewhere else
        delete _chans;
        delete _caps;
        Heap::free(_mounts);
    }
}

void VPE::init() {
    _caps->set(0);
    _caps->set(1);
    _chans->set(ChanMng::SYSC_CHAN);
    _chans->set(ChanMng::MEM_CHAN);
    _chans->set(ChanMng::DEF_RECVCHAN);
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

size_t VPE::alloc_chan() {
    size_t chan = _chans->first_clear();
    if(chan >= CHAN_COUNT)
        PANIC("No more free channels");
    _chans->set(chan);
    return chan;
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
    Syscalls::get().exchange(sel(), CapRngDesc(dest, crd.count()), crd, true);
}

int VPE::wait() {
    int exitcode;
    Syscalls::get().vpectrl(sel(), Syscalls::VCTRL_WAIT, 0, &exitcode);
    return exitcode;
}

}
