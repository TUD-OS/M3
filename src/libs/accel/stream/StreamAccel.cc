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

#include <base/log/Lib.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/File.h>
#include <m3/Syscalls.h>

#include <accel/stream/StreamAccel.h>

using namespace m3;

namespace accel {

const size_t StreamAccel::BUF_ADDR      = 0x6000;

VPE *StreamAccel::create(m3::PEISA isa, bool muxable) {
    VPE *acc = new VPE("acc", PEDesc(PEType::COMP_IMEM, isa), nullptr, muxable);
    if(Errors::last != Errors::NONE) {
        delete acc;
        acc = new VPE("acc", PEDesc(PEType::COMP_EMEM, isa), "pager", muxable);
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to find accelerator");
    }
    return acc;
}

uintptr_t StreamAccel::getRBAddr(const VPE &vpe) {
    if(vpe.pe().is_programmable())
        return 0;
    if(vpe.pe().has_memory())
        return vpe.pe().mem_size() - RB_SIZE;
    return RECVBUF_SPACE + SYSC_RBUF_SIZE + UPCALL_RBUF_SIZE;
}

void StreamAccel::sendUpdate(m3::SendGate &sgate, size_t off, uint64_t len) {
    UpdateCommand req;
    req.cmd = static_cast<uint64_t>(Command::UPDATE);
    req.off = off;
    req.len = len;
    req.eof = true;
    send_receive_msg(sgate, &req, sizeof(req));
}

uint64_t StreamAccel::update(m3::SendGate &sgate, m3::RecvGate &rgate, size_t off, uint64_t len) {
    sendUpdate(sgate, off, len);

    UpdateCommand req;
    size_t done = 0;
    do {
        m3::GateIStream is = receive_msg(rgate);
        is >> req;
        LLOG(ACCEL, "Finished off=" << req.off << ", len=" << req.len);
        done += req.len;
        reply_vmsg(is, 0);
    }
    while(!req.eof);
    return done;
}

Errors::Code StreamAccel::executeChain(RecvGate &rgate, File *in, File *out,
        ChainMember &first, ChainMember &last) {
    size_t inpos = 0, outpos = 0;
    size_t inlen = 0, outlen = 0;
    size_t inoff, outoff;
    capsel_t inmem, outmem, lastin = ObjCap::INVALID, lastout = ObjCap::INVALID;
    size_t last_out_off = static_cast<size_t>(-1);

    SendGate sgate = SendGate::create(&first.rgate);

    Errors::Code err;
    while(1) {
        // input depleted?
        if(inpos == inlen) {
            // request next memory cap for input
            if((err = in->read_next(&inmem, &inoff, &inlen)) != Errors::NONE)
                return err;

            LLOG(ACCEL, "input: sel=" << inmem << ", inoff=" << inoff << ", inlen=" << inlen);

            if(inlen == 0)
                break;

            inpos = 0;
            if(inmem != lastin) {
                MemGate::bind(inmem).activate_for(*first.vpe, EP_INPUT);
                lastin = inmem;
            }
        }

        // output depleted?
        if(outpos == outlen) {
            // request next memory cap for output
            if((err = out->begin_write(&outmem, &outoff, &outlen)) != Errors::NONE)
                return err;

            LLOG(ACCEL, "output: sel=" << outmem << ", outoff=" << outoff << ", outlen=" << outlen);

            outpos = 0;
        }

        // activate output mem with new offset
        if(outmem != lastout || last_out_off != outoff + outpos) {
            MemGate::bind(outmem).activate_for(*last.vpe, EP_OUTPUT, outoff + outpos);
            lastout = outmem;
            last_out_off = outoff + outpos;
        }

        // use the minimum of both, because input and output have to be of the same size atm
        size_t readsz = std::min(inlen - inpos, outlen - outpos);
        size_t writesz = update(sgate, rgate, inoff + inpos, readsz);

        LLOG(ACCEL, "commit_write(" << writesz << ")");

        inpos += readsz;
        outpos += writesz;
        out->commit_write(writesz);
    }

    // EOF
    update(sgate, rgate, 0, 0);
    return Errors::NONE;
}

}
