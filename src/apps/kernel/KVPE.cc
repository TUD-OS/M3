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

#include <m3/Common.h>

#include "KVPE.h"
#include "RecvBufs.h"
#include "PEManager.h"

namespace m3 {

KVPE::VPEId::VPEId(int _id, int _core) : id(_id), core(_core) {
    KDTU::get().set_vpeid(core, id);
}

KVPE::VPEId::~VPEId() {
    KDTU::get().unset_vpeid(core, id);
}

KVPE::KVPE(String &&prog, size_t id, bool bootmod, bool as, int ep, capsel_t pfgate)
    : RequestSessionData(), _id(id, id + APP_CORES), _daemon(), _bootmod(bootmod),
      _refs(0), _pid(), _state(DEAD), _exitcode(), _name(std::move(prog)),
      _objcaps(id + 1),
      _mapcaps(id + 1),
      _sepsgate(MemGate::bind(VPE::self().alloc_cap(), ObjCap::KEEP_CAP)),
      _syscgate(SyscallHandler::get().create_gate(this)),
      _srvgate(RecvGate::create(SyscallHandler::get().srvrcvbuf())),
      _as(as ? new AddrSpace(ep, pfgate) : nullptr),
      _requires(),
      _exitsubscr() {
    _objcaps.set(0, new VPECapability(&_objcaps, 0, this));
    _objcaps.set(1, new MemCapability(&_objcaps, 1, 0, (size_t)-1, MemGate::RWX, core(), id, 0));

    init();

    LOG(VPES, "Created VPE '" << _name << "' [id=" << id << ", pe=" << core() << "]");
    for(auto &r : _requires)
        LOG(VPES, "  requires: '" << r.name << "'");
}

void KVPE::unref() {
    // 1 because we always have a VPE-cap for ourself (not revokeable)
    if(--_refs == 1)
        PEManager::get().remove(id(), _daemon);
}

void KVPE::exit(int exitcode) {
    KDTU::get().invalidate_eps(*this);
    detach_rbufs();
    _state = DEAD;
    _exitcode = exitcode;
    for(auto it = _exitsubscr.begin(); it != _exitsubscr.end();) {
        auto cur = it++;
        cur->callback(exitcode, &*cur);
        _exitsubscr.unsubscribe(&*cur);
    }
}

  void KVPE::detach_rbufs() {
      for(size_t c = 0; c < EP_COUNT; ++c)
          RecvBufs::detach(*this, c);
  }

}
