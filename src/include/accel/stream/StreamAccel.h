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
#include <m3/com/GateStream.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

namespace m3 {
class File;
}

namespace accel {

/**
 * Defines and helper functions for stream accelerators
 */
class StreamAccel {
public:
    static const size_t MSG_SIZE    = 64;
    static const size_t RB_SIZE     = MSG_SIZE * 8;
    static const capsel_t RGATE_SEL = 2;
    static const capsel_t SGATE_SEL = 3;
    static const size_t EP_RECV     = 7;
    static const size_t EP_INPUT    = 8;
    static const size_t EP_OUTPUT   = 9;
    static const size_t EP_SEND     = 10;
    static const size_t EP_CTX      = 11;

    static const size_t STATE_SIZE  = 1024;
    static const size_t BUF_SIZE    = 8192;
    static const size_t BUF_ADDR;

    enum class Command {
        INIT,
        UPDATE,
    };

    struct InitCommand {
        uint64_t cmd;
        uint64_t out_size;
        uint64_t report_size;
        uint64_t comp_time;
    } PACKED;

    struct UpdateCommand {
        uint64_t cmd;
        uint64_t off;
        uint64_t len;
        uint64_t eof;
    } PACKED;

    struct ChainMember {
        explicit ChainMember(m3::VPE *_vpe, uintptr_t _rbuf, size_t rbSize, m3::RecvGate &rgdst, label_t label)
            : vpe(_vpe), rbuf(_rbuf),
              rgate(m3::RecvGate::create_for(*vpe, m3::getnextlog2(rbSize), m3::getnextlog2(MSG_SIZE))),
              sgate(m3::SendGate::create(&rgdst, label, rbSize)),
              spm(m3::MemGate::create_global(BUF_SIZE + STATE_SIZE, m3::MemGate::RW)) {
            m3::Syscalls::get().activate(vpe->sel(), spm.sel(), EP_CTX, 0);
        }
        ~ChainMember() {
            delete vpe;
        }

        void send_caps() {
            vpe->delegate(m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::OBJ, rgate.sel(), 1), RGATE_SEL);
            vpe->delegate(m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::OBJ, sgate.sel(), 1), SGATE_SEL);
        }

        void activate_recv() {
            if(rbuf)
                rgate.activate(EP_RECV, rbuf);
            else
                rgate.activate(EP_RECV);
        }

        void activate_send() {
            sgate.activate_for(*vpe, EP_SEND);
        }

        uintptr_t init(size_t outsize, size_t reportsize, cycles_t comptime) {
            InitCommand init;
            init.cmd = static_cast<int64_t>(Command::INIT);
            init.out_size = outsize;
            init.report_size = reportsize;
            init.comp_time = comptime;

            m3::SendGate sgate = m3::SendGate::create(&rgate);

            uintptr_t addr = BUF_ADDR;
            m3::GateIStream reply = send_receive_msg(sgate, &init, sizeof(init));
            if(reply.length() == sizeof(uintptr_t))
                reply >> addr;
            return addr;
        }

        m3::VPE *vpe;
        uintptr_t rbuf;
        m3::RecvGate rgate;
        m3::SendGate sgate;
        m3::MemGate spm;
    };

    /**
     * Creates an accelerator VPE, depending on which exists
     *
     * @param isa the ISA (fft, toupper)
     * @param muxable whether the VPE can be shared with others
     * @return the accelerator VPE
     */
    static m3::VPE *create(m3::PEISA isa, bool muxable);

    /**
     * @return the address of the receive buffer
     */
    static uintptr_t getRBAddr(const m3::VPE &vpe);

    /**
     * Sends the update request to the given send gate
     *
     * @param sgate the gate to send the update request to
     * @param off the current offset
     * @param len the length
     */
    static void sendUpdate(m3::SendGate &sgate, size_t off, uint64_t len);

    /**
     * Sends the update request and waits for the response.
     *
     * @param sgate the gate to send the update request to
     * @param rgate the gate ro receive the reply from
     * @param off the current offset
     * @param len the length
     * @return the returned length
     */
    static uint64_t update(m3::SendGate &sgate, m3::RecvGate &rgate, size_t off, uint64_t len);

    /**
     * Executes the given chain, i.e., connects <first> to <in> and <last> to <out> and handles
     * the protocol with the file objects.
     *
     * @param rgate the gate ro receive the reply from
     * @param in the input file
     * @param out the output file
     * @param first the first in the chain
     * @param last the last in the chain
     * @return the error code, if any
     */
    static m3::Errors::Code executeChain(m3::RecvGate &rgate, m3::File *in, m3::File *out,
        ChainMember &first, ChainMember &last);
};

}
