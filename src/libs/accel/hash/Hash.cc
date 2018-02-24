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

#include <accel/hash/Hash.h>

using namespace m3;

namespace accel {

Hash::Hash()
    : _accel(HashAccel::create()),
      _lastmem(ObjCap::INVALID),
      _rgate(RecvGate::create(nextlog2<256>::val, nextlog2<256>::val)),
      _srgate(RecvGate::create_for(_accel->vpe(), getnextlog2(HashAccel::RB_SIZE), getnextlog2(HashAccel::RB_SIZE))),
      _sgate(SendGate::create(&_srgate, 0, HashAccel::RB_SIZE, &_rgate)) {
    // has to be activated
    _rgate.activate();

    if(_accel->vpe().pager()) {
        goff_t virt = HashAccel::STATE_ADDR;
        _accel->vpe().pager()->map_anon(&virt, HashAccel::STATE_SIZE + HashAccel::BUF_SIZE, Pager::Prot::RW, 0);
    }

    _accel->vpe().delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _srgate.sel(), 1), HashAccel::RBUF);
    _srgate.activate(HashAccel::RECV_EP, _accel->getRBAddr());
    _accel->vpe().start();
}

Hash::~Hash() {
    delete _accel;
}

uint64_t Hash::sendRequest(HashAccel::Command cmd, uint64_t arg1, uint64_t arg2) {
    HashAccel::Request req;
    req.cmd = static_cast<uint64_t>(cmd);
    req.arg1 = arg1;
    req.arg2 = arg2;

    GateIStream is = send_receive_msg(_sgate, &req, sizeof(req));
    uint64_t res;
    is >> res;
    return res;
}

bool Hash::update(capsel_t mem, size_t offset, size_t len) {
    // that assumes that we never reuse a selector for a different capability
    if(_lastmem != mem) {
        Syscalls::get().activate(_accel->vpe().sel(), mem, HashAccel::DATA_EP, 0);
        _lastmem = mem;
    }

    return sendRequest(HashAccel::UPDATE, offset, len);
}

bool Hash::update(const void *data, size_t len) {
    const char *d = reinterpret_cast<const char*>(data);
    while(len > 0) {
        size_t amount = std::min(len, HashAccel::BUF_SIZE);
        _accel->vpe().mem().write(d, amount, HashAccel::BUF_ADDR);

        if(sendRequest(HashAccel::Command::UPDATE, 0, amount) != 1)
            return false;

        d += amount;
        len -= amount;
    }
    return true;
}

size_t Hash::finish(void *res, size_t max) {
    HashAccel::Request req;
    req.cmd = static_cast<uint64_t>(HashAccel::Command::FINISH);
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
    if(!start(false, algo))
        return 0;
    if(!update(data, len))
        return 0;
    return finish(res, max);
}

size_t Hash::get(Algorithm algo, m3::File *file, void *res, size_t max) {
    if(!start(true, algo))
        return 0;

    size_t offset, length;
    capsel_t inmem;
    while(1) {
        if(file->read_next(&inmem, &offset, &length) != Errors::NONE)
            return 0;
        if(length == 0)
            break;

        update(inmem, offset, length);
    }

    return finish(res, max);
}

size_t Hash::get_slow(Algorithm algo, m3::File *file, void *res, size_t max) {
    if(!start(false, algo))
        return 0;

    size_t offset, length;
    capsel_t inmem;
    char *buffer = new char[HashAccel::BUF_SIZE];
    while(1) {
        if(file->read_next(&inmem, &offset, &length) != Errors::NONE)
            return 0;
        if(length == 0)
            break;

        MemGate ingate = MemGate::bind(inmem);
        while(length > 0) {
            size_t amount = std::min(length, HashAccel::BUF_SIZE);
            ingate.read(buffer, amount, offset);

            update(buffer, amount);

            length -= amount;
            offset += amount;
        }
    }

    delete[] buffer;
    return finish(res, max);
}

}
