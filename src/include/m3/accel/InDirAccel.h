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
#include <m3/com/MemGate.h>
#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/VPE.h>

namespace m3 {

class InDirAccel {
public:
    static const size_t MSG_SIZE        = 64;

    static const size_t EP_RECV         = 4;
    static const size_t EP_OUT          = 5;
    static const capsel_t CAP_RECV      = 64;

    static const size_t BUF_ADDR        = 0x8000;
    static const size_t MAX_BUF_SIZE    = 32768;

    enum Operation {
        COMPUTE,
        FORWARD,
        IDLE,
    };

    struct InvokeMsg {
        uint64_t op;
        uint64_t dataSize;
        uint64_t compTime;
    } PACKED;

    explicit InDirAccel(VPE *vpe, RecvGate &reply_gate)
        : _mgate(),
          _rgate(RecvGate::create_for(*vpe, getnextlog2(MSG_SIZE), getnextlog2(MSG_SIZE))),
          _sgate(SendGate::create(&_rgate, 0, MSG_SIZE, &reply_gate)),
          _vpe(vpe) {
        // activate EP
        _rgate.activate(EP_RECV, vpe->pe().mem_size() - MSG_SIZE);
        // delegate cap
        vpe->delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _rgate.sel(), 1), CAP_RECV);
    }
    ~InDirAccel() {
        delete _mgate;
    }

    void connect_output(InDirAccel *accel) {
        _mgate = new MemGate(accel->_vpe->mem().derive(BUF_ADDR, MAX_BUF_SIZE));
        _mgate->activate_for(*_vpe, EP_OUT);
    }

    void read(void *data, size_t size) {
        assert(size <= MAX_BUF_SIZE);
        _vpe->mem().read(data, size, BUF_ADDR);
    }

    void write(const void *data, size_t size) {
        assert(size <= MAX_BUF_SIZE);
        _vpe->mem().write(data, size, BUF_ADDR);
    }

    void start(Operation op, size_t dataSize, cycles_t compTime, label_t reply_label) {
        InvokeMsg msg;
        msg.op = op;
        msg.dataSize = dataSize;
        msg.compTime = compTime;
        _sgate.send(&msg, sizeof(msg), reply_label);
    }

private:
    MemGate *_mgate;
    RecvGate _rgate;
    SendGate _sgate;
    VPE *_vpe;
};

}
