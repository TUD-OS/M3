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

#include <m3/accel/StreamAccel.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>

#include "accelchain.h"

using namespace m3;

void chain_direct(File *in, File *out, size_t num, cycles_t comptime) {
    VPE *vpes[num];
    StreamAccel *accels[num];

    // create VPEs
    for(size_t i = 0; i < num; ++i) {
        OStringStream name;
        name << "chain" << i;

        vpes[i] = new VPE(name.str(), PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_FFT), nullptr, false);
        if(Errors::last != Errors::NONE) {
            exitmsg("Unable to create VPE for " << name.str());
            break;
        }

        accels[i] = new StreamAccel(vpes[i], comptime);
    }

    // connect input/output
    accels[0]->connect_input(static_cast<GenericFile*>(in));
    accels[num - 1]->connect_output(static_cast<GenericFile*>(out));
    for(size_t i = 0; i < num; ++i) {
        if(i > 0)
            accels[i]->connect_input(accels[i - 1]);
        if(i + 1 < num)
            accels[i]->connect_output(accels[i + 1]);
    }

    // start VPEs
    for(size_t i = 0; i < num; ++i)
        vpes[i]->start();

    // wait for their completion
    capsel_t sels[num];
    for(size_t rem = num; rem > 0; --rem) {
        for(size_t x = 0, i = 0; i < num; ++i) {
            if(vpes[i])
                sels[x++] = vpes[i]->sel();
        }

        capsel_t vpe;
        int exitcode;
        if(Syscalls::get().vpewait(sels, rem, &vpe, &exitcode) != Errors::NONE)
            errmsg("Unable to wait for VPEs");
        else {
            for(size_t i = 0; i < num; ++i) {
                if(vpes[i] && vpes[i]->sel() == vpe) {
                    if(exitcode != 0) {
                        cerr << "chain" << i
                             << " terminated with exit code " << exitcode << "\n";
                    }
                    delete vpes[i];
                    delete accels[i];
                    vpes[i] = nullptr;
                    break;
                }
            }
        }
    }
}
