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
#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>

#include "accelchain.h"

using namespace m3;

static constexpr size_t PIPE_SHM_SIZE = 512 * 1024;

void chain_direct(File *in, File *out, size_t num, cycles_t comptime, Mode mode) {
    VPE *vpes[num];
    StreamAccel *accels[num];
    IndirectPipe *pipes[num];
    MemGate *mems[num];

    memset(pipes, 0, sizeof(pipes));
    memset(mems, 0, sizeof(mems));

    // create VPEs and put them into the same group
    VPEGroup group;
    for(size_t i = 0; i < num; ++i) {
        OStringStream name;
        name << "chain" << i;

        vpes[i] = new VPE(name.str(), PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_FFT), nullptr, true, &group);
        if(Errors::last != Errors::NONE) {
            exitmsg("Unable to create VPE for " << name.str());
            break;
        }

        accels[i] = new StreamAccel(vpes[i], comptime);

        if(mode == Mode::DIR_SIMPLE && i + 1 < num) {
            mems[i] = new MemGate(MemGate::create_global(PIPE_SHM_SIZE, MemGate::RW));
            pipes[i] = new IndirectPipe(*mems[i], PIPE_SHM_SIZE);
        }
    }

    // connect input/output
    accels[0]->connect_input(static_cast<GenericFile*>(in));
    accels[num - 1]->connect_output(static_cast<GenericFile*>(out));
    for(size_t i = 0; i < num; ++i) {
        if(i > 0) {
            if(mode == Mode::DIR_SIMPLE) {
                File *rd = VPE::self().fds()->get(pipes[i - 1]->reader_fd());
                accels[i]->connect_input(static_cast<GenericFile*>(rd));
            }
            else
                accels[i]->connect_input(accels[i - 1]);
        }
        if(i + 1 < num) {
            if(mode == Mode::DIR_SIMPLE) {
                File *wr = VPE::self().fds()->get(pipes[i]->writer_fd());
                accels[i]->connect_output(static_cast<GenericFile*>(wr));
            }
            else
                accels[i]->connect_output(accels[i + 1]);
        }
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
                    if(mode == Mode::DIR_SIMPLE) {
                        if(pipes[i])
                            pipes[i]->close_writer();
                        if(i > 0 && pipes[i - 1])
                            pipes[i - 1]->close_reader();
                    }
                    delete vpes[i];
                    delete accels[i];
                    vpes[i] = nullptr;
                    break;
                }
            }
        }
    }

    if(mode == Mode::DIR_SIMPLE) {
        for(size_t i = 0; i < num; ++i) {
            delete mems[i];
            delete pipes[i];
        }
    }
}
