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
#include <base/stream/IStringStream.h>
#include <base/util/Profile.h>
#include <base/PEDesc.h>

#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>
#include <m3/com/MemGate.h>
#include <m3/stream/Standard.h>

#include <accel/stream/Stream.h>

#include "swfilter.h"

using namespace m3;
using namespace accel;

int main() {
    RecvGate rg = RecvGate::bind(StreamAccel::RGATE_SEL, getnextlog2(StreamAccel::RB_SIZE));
    SendGate sg = SendGate::bind(StreamAccel::SGATE_SEL);
    MemGate out = MemGate::bind(ObjCap::INVALID);

    // gates are already activated
    rg.ep(StreamAccel::EP_RECV);
    sg.ep(StreamAccel::EP_SEND);
    out.ep(StreamAccel::EP_OUTPUT);

    size_t outSize = 0, reportSize = 0;
    ulong *buf = new ulong[SWFIL_BUF_SIZE / sizeof(ulong)];

    alignas(64) StreamAccel::UpdateCommand updcmd;
    updcmd.cmd = static_cast<uint64_t>(StreamAccel::Command::UPDATE);

    while(1) {
        GateIStream is = receive_msg(rg);
        uint64_t cmd;
        is >> cmd;

        if(static_cast<StreamAccel::Command>(cmd) == StreamAccel::Command::INIT) {
            auto *init = reinterpret_cast<const StreamAccel::InitCommand*>(is.message().data);
            outSize = init->out_size;
            reportSize = init->report_size;
            continue;
        }

        auto *upd = reinterpret_cast<const StreamAccel::UpdateCommand*>(is.message().data);
        // cout << "got off=" << fmt(upd->off, "#x") << " len=" << fmt(upd->len, "#x") << "\n";

        size_t agg = 0;
        size_t outOff = 0;
        size_t inOff = upd->off;
        size_t rem = upd->len;
        while(rem > 0) {
            size_t amount = Math::min(SWFIL_BUF_SIZE, rem);

            ulong *fl = reinterpret_cast<ulong*>(SWFIL_BUF_ADDR + inOff);
            size_t num = amount / sizeof(ulong);
            for(size_t i = 0; i < num; i += 4) {
                buf[i] = (fl[i] > 100) ? 0 : fl[i];
                buf[i + 1] = (fl[i + 1] > 100) ? 0 : fl[i + 1];
                buf[i + 2] = (fl[i + 2] > 100) ? 0 : fl[i + 2];
                buf[i + 3] = (fl[i + 3] > 100) ? 0 : fl[i + 3];
            }

            out.write(buf, amount, outOff);

            agg += amount;
            inOff += amount;
            rem -= amount;

            if((rem == 0 && upd->eof) || agg >= reportSize) {
                updcmd.off = outOff + amount - agg;
                updcmd.len = agg;
                updcmd.eof = rem == 0 && upd->eof;
                send_msg(sg, &updcmd, sizeof(updcmd));
                agg = 0;
            }

            outOff = (outOff + amount) % outSize;
        }
    }

    return 0;
}
