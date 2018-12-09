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

#include <base/stream/Serial.h>
#include <base/util/Time.h>

#include <m3/accel/StreamAccel.h>
#include <m3/stream/Standard.h>
#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>

#include "imgproc.h"

using namespace m3;

static constexpr bool VERBOSE           = 1;
static constexpr size_t PIPE_SHM_SIZE   = 512 * 1024;

static const char *names[] = {
    "FFT",
    "MUL",
    "IFFT",
};

class DirectChain {
public:
    static const size_t ACCEL_COUNT     = 3;

    explicit DirectChain(size_t id, File *in, File *out, Mode _mode)
        : mode(_mode),
          group(),
          vpes(),
          accels(),
          pipes(),
          mems() {
        // create VPEs and put them into the same group
        for(size_t i = 0; i < ACCEL_COUNT; ++i) {
            OStringStream name;
            name << names[i] << id;

            if(VERBOSE) Serial::get() << "Creating VPE " << name.str() << "\n";

            vpes[i] = new VPE(name.str(), PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_FFT),
                              nullptr, VPE::MUXABLE, &group);
            if(Errors::last != Errors::NONE) {
                exitmsg("Unable to create VPE for " << name.str());
                break;
            }

            accels[i] = new StreamAccel(vpes[i], ACCEL_TIMES[i]);

            if(mode == Mode::DIR_SIMPLE && i + 1 < ACCEL_COUNT) {
                mems[i] = new MemGate(MemGate::create_global(PIPE_SHM_SIZE, MemGate::RW));
                pipes[i] = new IndirectPipe(*mems[i], PIPE_SHM_SIZE);
            }
        }

        if(VERBOSE) Serial::get() << "Connecting input and output...\n";

        // connect input/output
        accels[0]->connect_input(static_cast<GenericFile*>(in));
        accels[ACCEL_COUNT - 1]->connect_output(static_cast<GenericFile*>(out));
        for(size_t i = 0; i < ACCEL_COUNT; ++i) {
            if(i > 0) {
                if(mode == Mode::DIR_SIMPLE) {
                    File *rd = VPE::self().fds()->get(pipes[i - 1]->reader_fd());
                    accels[i]->connect_input(static_cast<GenericFile*>(rd));
                }
                else
                    accels[i]->connect_input(accels[i - 1]);
            }
            if(i + 1 < ACCEL_COUNT) {
                if(mode == Mode::DIR_SIMPLE) {
                    File *wr = VPE::self().fds()->get(pipes[i]->writer_fd());
                    accels[i]->connect_output(static_cast<GenericFile*>(wr));
                }
                else
                    accels[i]->connect_output(accels[i + 1]);
            }
        }
    }
    ~DirectChain() {
        if(mode == Mode::DIR_SIMPLE) {
            for(size_t i = 0; i < ACCEL_COUNT; ++i) {
                delete mems[i];
                delete pipes[i];
            }
        }
        for(size_t i = 0; i < ACCEL_COUNT; ++i) {
            delete vpes[i];
            delete accels[i];
        }
    }

    void start() {
        for(size_t i = 0; i < ACCEL_COUNT; ++i) {
            vpes[i]->start();
            running[i] = true;
        }
    }

    void add_running(capsel_t *sels, size_t *count) {
        for(size_t i = 0; i < ACCEL_COUNT; ++i) {
            if(running[i])
                sels[(*count)++] = vpes[i]->sel();
        }
    }
    void terminated(capsel_t vpe, int exitcode) {
        for(size_t i = 0; i < ACCEL_COUNT; ++i) {
            if(running[i] && vpes[i]->sel() == vpe) {
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
                running[i] = false;
                break;
            }
        }
    }

private:
    Mode mode;
    VPEGroup group;
    VPE *vpes[ACCEL_COUNT];
    StreamAccel *accels[ACCEL_COUNT];
    IndirectPipe *pipes[ACCEL_COUNT];
    MemGate *mems[ACCEL_COUNT];
    bool running[ACCEL_COUNT];
};

static void wait_for(DirectChain **chains, size_t num) {
    for(size_t rem = num * DirectChain::ACCEL_COUNT; rem > 0; --rem) {
        size_t count = 0;
        capsel_t sels[num * DirectChain::ACCEL_COUNT];
        for(size_t i = 0; i < num; ++i)
            chains[i]->add_running(sels, &count);

        capsel_t vpe;
        int exitcode;
        if(Syscalls::get().vpewait(sels, rem, &vpe, &exitcode) != Errors::NONE)
            errmsg("Unable to wait for VPEs");
        else {
            for(size_t i = 0; i < num; ++i)
                chains[i]->terminated(vpe, exitcode);
        }
    }
}

void chain_direct(const char *in, size_t num, Mode mode) {
    DirectChain *chains[num];
    fd_t infds[num];
    fd_t outfds[num];

    // create <num> chains
    for(size_t i = 0; i < num; ++i) {
        OStringStream outpath;
        outpath << "/tmp/res-" << i;

        infds[i] = VFS::open(in, FILE_R);
        if(infds[i] == FileTable::INVALID)
            exitmsg("Unable to open " << in);
        outfds[i] = VFS::open(outpath.str(), FILE_W | FILE_TRUNC | FILE_CREATE);
        if(outfds[i] == FileTable::INVALID)
            exitmsg("Unable to open " << outpath.str());

        chains[i] = new DirectChain(i,
                                    VPE::self().fds()->get(infds[i]),
                                    VPE::self().fds()->get(outfds[i]),
                                    mode);
    }

    if(VERBOSE) Serial::get() << "Starting chain...\n";

    cycles_t start = Time::start(0);

    if(mode == Mode::DIR) {
        for(size_t i = 0; i < num; ++i)
            chains[i]->start();
        wait_for(chains, num);
    }
    else {
        for(size_t i = 0; i < num / 2; ++i)
            chains[i]->start();
        wait_for(chains, num / 2);
        for(size_t i = num / 2; i < num; ++i)
            chains[i]->start();
        wait_for(chains + num / 2, num / 2);
    }

    cycles_t end = Time::stop(0);
    Serial::get() << "Total time: " << (end - start) << " cycles\n";

    // cleanup
    for(size_t i = 0; i < num; ++i) {
        VFS::close(infds[i]);
        VFS::close(outfds[i]);
        delete chains[i];
    }
}
