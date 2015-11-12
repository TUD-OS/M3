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

#include <m3/Common.h>

#include "KVPE.h"
#include "PEManager.h"

namespace m3 {

KVPE::KVPE(String &&prog, size_t id)
    : RequestSessionData(), _id(id), _refs(0), _pid(), _state(DEAD), _exitcode(), _name(std::move(prog)),
      _caps(id + 1),
      _sepsgate(MemGate::bind(VPE::self().alloc_cap(), Cap::KEEP_CAP)),
      _syscgate(SyscallHandler::get().create_gate(this)),
      _srvgate(RecvGate::create(SyscallHandler::get().srvrcvbuf())), _requires() {
    _caps.set(0, new VPECapability(this));
    _caps.set(1, new MemCapability(0, (size_t)-1, MemGate::RWX, core(), 0));

    LOG(VPES, "Created VPE '" << _name << "' [id=" << _id << "]");
    for(auto &r : _requires)
        LOG(VPES, "  requires: '" << r.name << "'");
}

void KVPE::unref() {
    // 1 because we always have a VPE-cap for ourself (not revokeable)
    if(--_refs == 1)
        PEManager::get().remove(_id);
}

void KVPE::exit(int exitcode) {
    KDTU::get().invalidate_eps(core());
    detach_rbufs();
    _state = DEAD;
    _exitcode = exitcode;
    for(auto it = _exitsubscr.begin(); it != _exitsubscr.end();) {
        auto cur = it++;
        cur->callback(exitcode, &*cur);
        _exitsubscr.unsubscribe(&*cur);
    }
}

}
