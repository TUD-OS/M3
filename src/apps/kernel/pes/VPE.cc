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
#include "pes/VPE.h"
#include "pes/PEManager.h"
#include "Platform.h"

namespace kernel {

VPE::VPEId::VPEId(int id, int core) : desc(core, id) {
    DTU::get().set_vpeid(desc);
}

VPE::VPEId::~VPEId() {
    DTU::get().unset_vpeid(desc);
}

VPE::VPE(m3::String &&prog, size_t id, bool bootmod, int ep, capsel_t pfgate)
    : _id(id, id), _flags(bootmod ? BOOTMOD : 0),
      _refs(0), _pid(), _state(DEAD), _exitcode(), _name(std::move(prog)),
      _objcaps(id + 1),
      _mapcaps(id + 1),
      _eps(),
      _syscgate(SyscallHandler::get().create_gate(this)),
      _srvgate(SyscallHandler::get().srvepid(), nullptr),
      _as(Platform::pe(core()).has_virtmem() ? new AddrSpace(ep, pfgate) : nullptr),
      _requires(),
      _exitsubscr() {
    _objcaps.set(0, new VPECapability(&_objcaps, 0, this));
    _objcaps.set(1, new MemCapability(&_objcaps, 1, 0, MEMCAP_END, m3::KIF::Perm::RWX, core(), id, 0));

    init();

    KLOG(VPES, "Created VPE '" << _name << "' [id=" << id << ", pe=" << core() << "]");
    for(auto &r : _requires)
        KLOG(VPES, "  requires: '" << r.name << "'");
}

void VPE::unref() {
    // 1 because we always have a VPE-cap for ourself (not revokeable)
    if(--_refs == 1)
        PEManager::get().remove(id(), _flags & DAEMON);
}

void VPE::exit(int exitcode) {
    DTU::get().invalidate_eps(desc(), m3::DTU::FIRST_FREE_EP);
    detach_rbufs(false);
    _state = DEAD;
    _exitcode = exitcode;
    for(auto it = _exitsubscr.begin(); it != _exitsubscr.end();) {
        auto cur = it++;
        cur->callback(exitcode, &*cur);
        _exitsubscr.unsubscribe(&*cur);
    }
}

void VPE::detach_rbufs(bool all) {
    for(size_t c = 0; c < EP_COUNT; ++c) {
        if(!all && c == m3::DTU::DEF_RECVEP)
            continue;
        RecvBufs::detach(*this, c);
    }
}

}
