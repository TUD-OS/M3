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

#include <m3/com/GateStream.h>
#include <m3/stream/Standard.h>
#include <m3/session/Pager.h>
#include <m3/Syscalls.h>

#include <hash/Hash.h>

using namespace m3;

namespace hash {

Hash::Hash()
    : _accel(Accel::create()),
      _srgate(RecvGate::create_for(_accel->get(), getnextlog2(hash::Accel::RB_SIZE), getnextlog2(hash::Accel::RB_SIZE))),
      _crgate(RecvGate::create(nextlog2<256>::val, nextlog2<256>::val)),
      _send(SendGate::create(&_srgate, Accel::BUF_ADDR, 256, &_crgate)) {
    // has to be activated
    _crgate.activate();

    if(_accel->get().pager()) {
        uintptr_t virt = BUF_ADDR;
        _accel->get().pager()->map_anon(&virt, Accel::BUF_SIZE, Pager::Prot::RW, 0);
    }

    _accel->get().delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _srgate.sel(), 1), hash::Accel::RBUF);
    _srgate.activate(hash::Accel::EPID, _accel->getRBAddr());
    _accel->get().start();
}

Hash::~Hash() {
    delete _accel;
}

size_t Hash::get(Algorithm algo, const void *data, size_t len, void *res, size_t max) {
    assert(len <= Accel::BUF_SIZE);
    _accel->get().mem().write_sync(data, len, BUF_ADDR);

    Accel::Request req;
    req.algo = algo;
    req.len = len;
    GateIStream is = send_receive_msg(_send, &req, sizeof(req));

    uint64_t count;
    is >> count;

    if(count == 0)
        return 0;
    memcpy(res, is.buffer() + sizeof(uint64_t), Math::min(max, static_cast<size_t>(count)));
    return count;
}

}
