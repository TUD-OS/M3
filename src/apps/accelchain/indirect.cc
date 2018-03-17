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
#include <base/util/Time.h>
#include <base/PEDesc.h>

#include <m3/accel/InDirAccel.h>
#include <m3/stream/Standard.h>
#include <m3/session/Pager.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>

using namespace m3;

#include "accelchain.h"

static const size_t BUF_SIZE    = 8192;
static const size_t REPLY_SIZE  = 64;

void chain_indirect(File *in, File *out, size_t num, cycles_t comptime) {
    uint8_t *buffer = new uint8_t[BUF_SIZE];

    VPE *vpes[num];
    InDirAccel *accels[num];

    RecvGate reply_gate = RecvGate::create(getnextlog2(REPLY_SIZE * num), nextlog2<REPLY_SIZE>::val);
    reply_gate.activate();

    // create VPEs
    for(size_t i = 0; i < num; ++i) {
        OStringStream name;
        name << "chain" << i;

        vpes[i] = new VPE(name.str(), PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_INDIR), nullptr, false);
        if(Errors::last != Errors::NONE) {
            exitmsg("Unable to create VPE for " << name.str());
            break;
        }

        accels[i] = new InDirAccel(vpes[i], reply_gate);
    }

    // connect outputs
    for(size_t i = 0; i < num - 1; ++i)
        accels[i]->connect_output(accels[i + 1]);

    // start VPEs
    for(size_t i = 0; i < num; ++i)
        vpes[i]->start();

    size_t total = 0, seen = 0;
    ssize_t count = in->read(buffer, BUF_SIZE);
    if(count < 0)
        goto error;

    // label 0 is special; use 1..n
    accels[0]->write(buffer, static_cast<size_t>(count));
    accels[0]->start(static_cast<size_t>(count), comptime, 1);
    total += static_cast<size_t>(count);

    count = in->read(buffer, BUF_SIZE);

    while(seen < total) {
        label_t label;
        size_t written;

        // ack the message immediately
        {
            GateIStream is = receive_msg(reply_gate);
            label = is.label<label_t>() - 1;
            is >> written;
        }

        // cout << "got msg from " << label << "\n";

        if(label == num - 1) {
            accels[num - 1]->read(buffer, written);
            // cout << "write " << written << " bytes\n";
            out->write(buffer, written);
            seen += written;
        }

        if(label == 0) {
            if(num > 1)
                accels[1]->start(written, comptime, 2);

            total += static_cast<size_t>(count);
            if(count > 0) {
                accels[0]->write(buffer, static_cast<size_t>(count));
                accels[0]->start(static_cast<size_t>(count), comptime, 1);

                count = in->read(buffer, BUF_SIZE);
                // cout << "read " << count << " bytes\n";
                if(count < 0)
                    goto error;
            }
        }
        else if(label != num - 1)
            accels[label + 1]->start(written, comptime, label + 1 + 1);

        // cout << seen << " / " << total << "\n";
    }

error:
    for(size_t i = 0; i < num; ++i) {
        delete accels[i];
        delete vpes[i];
    }
    delete[] buffer;
}
