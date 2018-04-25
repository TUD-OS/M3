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

#pragma once

#include <m3/com/MemGate.h>
#include <m3/com/SendGate.h>
#include <m3/com/RecvGate.h>
#include <m3/vfs/GenericFile.h>
#include <m3/VPE.h>

namespace m3 {

class StreamAccel {
    struct Context {
        uint32_t flags;
        uint32_t masks;
        uint32_t outLenMask[2];
        uint64_t compTime;
        uint64_t msgAddr;
        uint64_t inReqAddr;
        uint64_t outReqAddr;
        uint64_t commitOff;
        uint64_t commitLen;
        uint64_t inOff;
        uint64_t inPos;
        uint64_t inLen;
        uint64_t outOff;
        uint64_t outPos;
        uint64_t outLen;
        uint64_t lastSize;
        uint64_t nextSysc;
    } PACKED;

public:
    static const size_t MSG_SIZE    = 64;
    static const size_t RB_SIZE     = MSG_SIZE * 4;

    static const capsel_t CAP_IN    = 64;
    static const capsel_t CAP_OUT   = 65;
    static const capsel_t CAP_RECV  = 66;

    static const size_t EP_RECV     = 5;
    static const size_t EP_IN_SEND  = 6;
    static const size_t EP_IN_MEM   = 7;
    static const size_t EP_OUT_SEND = 8;
    static const size_t EP_OUT_MEM  = 9;
    static const size_t EP_CTX      = 10;

    static const uint64_t LBL_IN_REQ    = 1;
    static const uint64_t LBL_IN_REPLY  = 2;
    static const uint64_t LBL_OUT_REQ   = 3;
    static const uint64_t LBL_OUT_REPLY = 4;

    static const size_t BUF_ADDR    = 0x8000;
    static const size_t BUF_SIZE    = 8192;

    explicit StreamAccel(VPE *vpe, cycles_t compTime)
        : _sgate(),
          _mgate(),
          _rgate(RecvGate::create_for(*vpe, getnextlog2(RB_SIZE), getnextlog2(MSG_SIZE))),
          _spm(MemGate::create_global(BUF_SIZE + sizeof(Context), MemGate::RW)),
          _vpe(vpe) {
        // activate EPs
        _rgate.activate(EP_RECV, vpe->pe().mem_size() - RB_SIZE);
        _spm.activate_for(*vpe, EP_CTX);
        // delegate caps
        vpe->delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _rgate.sel(), 1), CAP_RECV);

        // init the state
        Context ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.compTime = compTime;
        _spm.write(&ctx, sizeof(ctx), 0);
    }
    ~StreamAccel() {
        delete _sgate;
        delete _mgate;
    }

    void connect_input(GenericFile *file) {
        connect_file(file, EP_IN_SEND, EP_IN_MEM, CAP_IN);
    }
    void connect_input(StreamAccel *prev) {
        _sgate = new SendGate(SendGate::create(&prev->_rgate, LBL_IN_REQ, MSG_SIZE));
        _sgate->activate_for(*_vpe, EP_IN_SEND);
        _vpe->delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _sgate->sel()), CAP_IN);
    }

    void connect_output(GenericFile *file) {
        connect_file(file, EP_OUT_SEND, EP_OUT_MEM, CAP_OUT);
    }
    void connect_output(StreamAccel *next) {
        _sgate = new SendGate(SendGate::create(&next->_rgate, LBL_OUT_REQ, MSG_SIZE));
        _sgate->activate_for(*_vpe, EP_OUT_SEND);
        _mgate = new MemGate(next->_vpe->mem().derive(BUF_ADDR, BUF_SIZE));
        _mgate->activate_for(*_vpe, EP_OUT_MEM);
        _vpe->delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, _sgate->sel()), CAP_OUT);
    }

private:
    void connect_file(GenericFile *file, epid_t sep, epid_t mep, capsel_t scap) {
        file->sgate().activate_for(*_vpe, sep);
        file->sess().delegate_obj(_vpe->ep_sel(mep));
        _vpe->delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, file->sgate().sel()), scap);
    }

    SendGate *_sgate;
    MemGate *_mgate;
    RecvGate _rgate;
    MemGate _spm;
    VPE *_vpe;
};

}
