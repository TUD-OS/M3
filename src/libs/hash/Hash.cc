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

Hash::DirectBackend::DirectBackend()
    : accel(Accel::create()),
      srgate(RecvGate::create_for(accel->vpe(), getnextlog2(hash::Accel::RB_SIZE), getnextlog2(hash::Accel::RB_SIZE))) {
    if(accel->vpe().pager()) {
        uintptr_t virt = Accel::STATE_ADDR;
        accel->vpe().pager()->map_anon(&virt, Accel::STATE_SIZE + Accel::BUF_SIZE, Pager::Prot::RW, 0);
    }

    accel->vpe().delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, srgate.sel(), 1), hash::Accel::RBUF);
    srgate.activate(hash::Accel::RECV_EP, accel->getRBAddr());
    accel->vpe().start();
}

Hash::DirectBackend::~DirectBackend() {
    delete accel;
}

Hash::IndirectBackend::IndirectBackend(const char *service)
    : sess(service) {
}

Hash::Hash()
    : _backend(new DirectBackend()),
      _rgate(RecvGate::create(nextlog2<256>::val, nextlog2<256>::val)),
      _sgate(SendGate::create(&static_cast<DirectBackend*>(_backend)->srgate, 0, hash::Accel::RB_SIZE, &_rgate)),
      _mgate(MemGate::bind(static_cast<DirectBackend*>(_backend)->accel->vpe().mem().sel())),
      _memoff(Accel::BUF_ADDR) {
    // has to be activated
    _rgate.activate();
}

Hash::Hash(const char *service)
    : _backend(new IndirectBackend(service)),
      _rgate(RecvGate::create(nextlog2<256>::val, nextlog2<256>::val)),
      _sgate(SendGate::bind(static_cast<IndirectBackend*>(_backend)->sess.obtain(1).start(), &_rgate)),
      _mgate(MemGate::bind(static_cast<IndirectBackend*>(_backend)->sess.obtain(1).start())),
      _memoff(0) {
    _rgate.activate();
}

Hash::~Hash() {
    delete _backend;
}

uint64_t Hash::sendRequest(Accel::Command cmd, uint64_t arg1, uint64_t arg2) {
    Accel::Request req;
    req.cmd = static_cast<uint64_t>(cmd);
    req.arg1 = arg1;
    req.arg2 = arg2;

    GateIStream is = send_receive_msg(_sgate, &req, sizeof(req));
    uint64_t res;
    is >> res;
    return res;
}

bool Hash::update(const void *data, size_t len, bool write) {
    const char *d = reinterpret_cast<const char*>(data);
    while(len > 0) {
        size_t amount = std::min(len, Accel::BUF_SIZE);
        if(write)
            _mgate.write(d, amount, _memoff);

        if(sendRequest(Accel::Command::UPDATE, amount, Accel::BUF_ADDR) != 1)
            return false;

        d += amount;
        len -= amount;
    }
    return true;
}

size_t Hash::finish(void *res, size_t max) {
    Accel::Request req;
    req.cmd = static_cast<uint64_t>(Accel::Command::FINISH);
    req.arg1 = 0;
    req.arg2 = 0;
    GateIStream is = send_receive_msg(_sgate, &req, sizeof(req));

    uint64_t count;
    is >> count;
    if(count == 0)
        return 0;
    memcpy(res, is.buffer() + sizeof(uint64_t), Math::min(max, static_cast<size_t>(count)));
    return count;
}

size_t Hash::get(Algorithm algo, const void *data, size_t len, void *res, size_t max) {
    if(!start(algo))
        return 0;
    if(!update(data, len))
        return 0;
    return finish(res, max);
}

}
