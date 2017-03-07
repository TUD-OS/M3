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

#include <accel/stream/Stream.h>

using namespace m3;

namespace accel {

Stream::Stream(PEISA isa)
    : _accel(StreamAccel::create(isa)),
      _rgate(RecvGate::create(nextlog2<256>::val, nextlog2<256>::val)),
      _srgate(RecvGate::create_for(_accel->vpe(), getnextlog2(StreamAccel::RB_SIZE), getnextlog2(StreamAccel::RB_SIZE))),
      _sgate(SendGate::create(&_srgate, 0, StreamAccel::RB_SIZE, &_rgate)) {
    // has to be activated
    _rgate.activate();

    if(_accel->vpe().pager()) {
        uintptr_t virt = StreamAccel::BUF_ADDR;
        _accel->vpe().pager()->map_anon(&virt, StreamAccel::BUF_SIZE, Pager::Prot::RW, 0);
    }

    _accel->vpe().delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _srgate.sel(), 1), StreamAccel::RBUF_SEL);
    _srgate.activate(StreamAccel::EP_RECV, _accel->getRBAddr());

    _accel->vpe().start();
}

Stream::~Stream() {
    delete _accel;
}

uint64_t Stream::sendRequest(uint64_t inoff, uint64_t outoff, uint64_t len, bool autonomous) {
    StreamAccel::Request req;
    req.inoff = inoff;
    req.outoff = outoff;
    req.len = len;
    req.autonomous = autonomous;

    GateIStream is = send_receive_msg(_sgate, &req, sizeof(req));
    uint64_t res;
    is >> res;
    return res;
}

Errors::Code Stream::execute(File *in, File *out) {
    size_t inpos = 0, outpos = 0;
    size_t inlen = 0, outlen = 0;
    size_t inoff, outoff;
    capsel_t inmem, outmem;
    capsel_t lastin = ObjCap::INVALID, lastout = ObjCap::INVALID;

    Errors::Code err;
    while(1) {
        // input depleted?
        if(inpos == inlen) {
            // request next memory cap for input
            if((err = in->read_next(&inmem, &inoff, &inlen)) != Errors::NONE)
                return err;

            if(inlen == 0)
                break;

            // activate it, if necessary
            inpos = 0;
            if(inmem != lastin) {
                Syscalls::get().activate(_accel->vpe().sel(), inmem, StreamAccel::EP_INPUT, 0);
                lastin = inmem;
            }
        }

        // output depleted?
        if(outpos == outlen) {
            // request next memory cap for output
            if((err = out->begin_write(&outmem, &outoff, &outlen)) != Errors::NONE)
                return err;

            // activate it, if necessary
            outpos = 0;
            if(outmem != lastout) {
                Syscalls::get().activate(_accel->vpe().sel(), outmem, StreamAccel::EP_OUTPUT, 0);
                lastout = outmem;
            }
        }

        // use the minimum of both, because input and output have to be of the same size atm
        size_t amount = std::min(inlen - inpos, outlen - outpos);
        if(sendRequest(inoff + inpos, outoff + outpos, amount, true) != 0)
            return Errors::INV_ARGS;

        inpos += amount;
        outpos += amount;
        out->commit_write(amount);
    }

    return Errors::NONE;
}

Errors::Code Stream::execute_slow(File *in, File *out) {
    Errors::Code res = Errors::NONE;
    char *buffer = new char[StreamAccel::BUF_SIZE];

    ssize_t inlen;
    while((inlen = in->read(buffer, StreamAccel::BUF_SIZE)) > 0) {
        size_t amount = static_cast<size_t>(inlen);

        _accel->vpe().mem().write(buffer, amount, StreamAccel::BUF_ADDR);

        if(sendRequest(0, 0, amount, false) != 0) {
            res = Errors::INV_ARGS;
            goto error;
        }

        _accel->vpe().mem().read(buffer, amount, StreamAccel::BUF_ADDR);
        out->write(buffer, amount);
    }

error:
    delete[] buffer;
    return res;
}

}
