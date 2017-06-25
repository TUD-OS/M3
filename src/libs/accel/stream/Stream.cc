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
      _argate(RecvGate::create_for(_accel->vpe(), getnextlog2(StreamAccel::RB_SIZE),
                                                  getnextlog2(StreamAccel::MSG_SIZE))),
      _asgate(SendGate::create(&_rgate)),
      _sgate(SendGate::create(&_argate)) {
    // has to be activated
    _rgate.activate();

    if(_accel->vpe().pager()) {
        uintptr_t virt = StreamAccel::BUF_ADDR;
        _accel->vpe().pager()->map_anon(&virt, StreamAccel::BUF_MAX_SIZE, Pager::Prot::RW, 0);
    }

    // delegate send and receive gate
    _accel->vpe().delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _argate.sel(), 1), StreamAccel::RGATE_SEL);
    _accel->vpe().delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _asgate.sel(), 1), StreamAccel::SGATE_SEL);

    // activate them
    _argate.activate(StreamAccel::EP_RECV, _accel->getRBAddr());
    _asgate.activate_for(_accel->vpe(), StreamAccel::EP_SEND);

    _accel->vpe().start();
}

Stream::~Stream() {
    delete _accel;
}

void Stream::sendInit(size_t bufsize, size_t outsize, size_t reportsize) {
    StreamAccel::InitCommand init;
    init.cmd = static_cast<int64_t>(StreamAccel::Command::INIT);
    init.buf_size = bufsize;
    init.out_size = outsize;
    init.report_size = reportsize;

    send_msg(_sgate, &init, sizeof(init));
}

uint64_t Stream::sendRequest(uint64_t off, uint64_t len) {
    StreamAccel::UpdateCommand req;
    req.cmd = static_cast<uint64_t>(StreamAccel::Command::UPDATE);
    req.off = off;
    req.len = len;
    req.eof = true;
    send_msg(_sgate, &req, sizeof(req));

    size_t done = 0;
    while(done < len) {
        GateIStream is = receive_msg(_rgate);
        is >> req;
        // cout << "Finished off=" << req.off << ", len=" << req.len << "\n";
        done += req.len;
    }
    return done;
}

Errors::Code Stream::execute(File *in, File *out, size_t bufsize) {
    size_t inpos = 0, outpos = 0;
    size_t inlen = 0, outlen = 0;
    size_t inoff, outoff;
    capsel_t inmem, outmem, lastin = ObjCap::INVALID;

    sendInit(bufsize, static_cast<size_t>(-1), static_cast<size_t>(-1));

    Errors::Code err = Errors::NONE;
    while(1) {
        // input depleted?
        if(inpos == inlen) {
            // request next memory cap for input
            if((err = in->read_next(&inmem, &inoff, &inlen)) != Errors::NONE)
                return err;

            if(inlen == 0)
                break;

            inpos = 0;
            if(inmem != lastin) {
                MemGate::bind(inmem).activate_for(_accel->vpe(), StreamAccel::EP_INPUT);
                lastin = inmem;
            }
        }

        // output depleted?
        if(outpos == outlen) {
            // request next memory cap for output
            if((err = out->begin_write(&outmem, &outoff, &outlen)) != Errors::NONE)
                return err;

            outpos = 0;
        }

        // activate output mem with new offset
        MemGate::bind(outmem).activate_for(_accel->vpe(), StreamAccel::EP_OUTPUT, outoff + outpos);

        // use the minimum of both, because input and output have to be of the same size atm
        size_t amount = std::min(inlen - inpos, outlen - outpos);
        amount = sendRequest(inoff + inpos, amount);

        inpos += amount;
        outpos += amount;
        out->commit_write(amount);
    }

    return Errors::NONE;
}

Errors::Code Stream::execute_slow(File *in, File *out, size_t bufsize) {
    Errors::Code res = Errors::NONE;
    uint8_t *buffer = new uint8_t[bufsize];

    sendInit(bufsize, static_cast<size_t>(-1), static_cast<size_t>(-1));

    ssize_t inlen;
    while((inlen = in->read(buffer, bufsize)) > 0) {
        size_t amount = static_cast<size_t>(inlen);

        _accel->vpe().mem().write(buffer, amount, StreamAccel::BUF_ADDR);

        amount = sendRequest(0, amount);

        _accel->vpe().mem().read(buffer, amount, StreamAccel::BUF_ADDR);
        out->write(buffer, amount);
    }

    delete[] buffer;
    return res;
}

}
