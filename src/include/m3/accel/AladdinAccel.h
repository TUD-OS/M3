/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#pragma once

#include <m3/com/GateStream.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/session/Pager.h>
#include <m3/VPE.h>

namespace m3 {

class Aladdin {
public:
    static const uint RBUF_SEL      = 64;
    static const uint RECV_EP       = 7;
    static const uint MEM_EP        = 8;
    static const uint DATA_EP       = 9;
    static const size_t RB_SIZE     = 256;

    static const size_t BUF_SIZE    = 1024;
    static const size_t BUF_ADDR    = 0x8000;
    static const size_t STATE_SIZE  = 1024;
    static const size_t STATE_ADDR  = BUF_ADDR - STATE_SIZE;
    static const size_t RBUF_ADDR   = RECVBUF_SPACE + SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE + DEF_RBUF_SIZE;

    struct Array {
        uint64_t addr;
        uint64_t size;
    } PACKED;

    struct InvokeMessage {
        Array arrays[8];
        uint64_t array_count;
        uint64_t iterations;
        uint64_t repeats;
    } PACKED;

    explicit Aladdin(PEISA isa)
        : _accel(new VPE("aladdin", PEDesc(PEType::COMP_EMEM, isa), "pager", VPE::MUXABLE)),
          _lastmem(ObjCap::INVALID),
          _rgate(RecvGate::create(nextlog2<256>::val, nextlog2<256>::val)),
          _srgate(RecvGate::create_for(*_accel, getnextlog2(RB_SIZE), getnextlog2(RB_SIZE))),
          _sgate(SendGate::create(&_srgate, 0, RB_SIZE, &_rgate)) {
        // has to be activated
        _rgate.activate();

        if(_accel->pager()) {
            _accel->pager()->activate_gates(*_accel);
            goff_t virt = STATE_ADDR;
            _accel->pager()->map_anon(&virt, STATE_SIZE + BUF_SIZE, Pager::Prot::RW, 0);
        }

        _accel->delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _srgate.sel(), 1), RBUF_SEL);
        _srgate.activate(RECV_EP, RBUF_ADDR);
        _accel->start();
    }
    ~Aladdin() {
        delete _accel;
    }

    PEISA isa() const {
        return _accel->pe().isa();
    }
    void start(const InvokeMessage &msg) {
        send_msg(_sgate, &msg, sizeof(msg));
    }
    uint64_t wait() {
        GateIStream is = receive_reply(_sgate);
        uint64_t res;
        is >> res;
        return res;
    }
    uint64_t invoke(const InvokeMessage &msg) {
        start(msg);
        return wait();
    }

    VPE *_accel;
    capsel_t _lastmem;
    m3::RecvGate _rgate;
    m3::RecvGate _srgate;
    m3::SendGate _sgate;
};

}
