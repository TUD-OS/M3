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

#include <base/Common.h>

#include <m3/stream/Standard.h>
#include <m3/VPE.h>

#include <accel/stream/Stream.h>

struct ChainMember {
    explicit ChainMember(m3::VPE *_vpe, uintptr_t _rbuf, size_t rbSize, m3::RecvGate &rgdst, label_t label)
        : vpe(_vpe), rbuf(_rbuf),
          rgate(m3::RecvGate::create_for(*vpe, m3::getnextlog2(rbSize),
                                              m3::getnextlog2(accel::StreamAccelVPE::MSG_SIZE))),
          sgate(m3::SendGate::create(&rgdst, label, rbSize)) {
    }
    ~ChainMember() {
        delete vpe;
    }

    void send_caps() {
        vpe->delegate(m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::OBJ, rgate.sel(), 1),
            accel::StreamAccelVPE::RGATE_SEL);
        vpe->delegate(m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::OBJ, sgate.sel(), 1),
            accel::StreamAccelVPE::SGATE_SEL);
    }

    void activate_recv() {
        if(rbuf)
            rgate.activate(accel::StreamAccelVPE::EP_RECV, rbuf);
        else
            rgate.activate(accel::StreamAccelVPE::EP_RECV);
    }

    void activate_send() {
        sgate.activate_for(*vpe, accel::StreamAccelVPE::EP_SEND);
    }

    void init(size_t bufsize, size_t outsize, size_t reportsize, cycles_t comptime) {
        accel::StreamAccelVPE::InitCommand init;
        init.cmd = static_cast<int64_t>(accel::StreamAccelVPE::Command::INIT);
        init.buf_size = bufsize;
        init.out_size = outsize;
        init.report_size = reportsize;
        init.comp_time = comptime;

        m3::SendGate sgate = m3::SendGate::create(&rgate);
        send_receive_msg(sgate, &init, sizeof(init));
    }

    m3::VPE *vpe;
    uintptr_t rbuf;
    m3::RecvGate rgate;
    m3::SendGate sgate;
};

static inline void sendRequest(m3::SendGate &sgate, size_t off, uint64_t len) {
    accel::StreamAccelVPE::UpdateCommand req;
    req.cmd = static_cast<uint64_t>(accel::StreamAccelVPE::Command::UPDATE);
    req.off = off;
    req.len = len;
    req.eof = true;
    send_receive_msg(sgate, &req, sizeof(req));
}

static inline uint64_t requestResponse(m3::SendGate &sgate, m3::RecvGate &rgate, size_t off, uint64_t len) {
    sendRequest(sgate, off, len);

    size_t done = 0;
    while(done < len) {
        accel::StreamAccelVPE::UpdateCommand req;
        m3::GateIStream is = receive_msg(rgate);
        is >> req;
        m3::cout << "Finished off=" << req.off << ", len=" << req.len << "\n";
        done += req.len;
        reply_vmsg(is, 0);
    }
    return done;
}
