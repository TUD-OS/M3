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

#include <base/stream/Serial.h>
#include <base/util/Time.h>

#include <m3/accel/StreamAccel.h>
#include <m3/stream/Standard.h>
#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>

#include "lte.h"

using namespace m3;

static constexpr bool VERBOSE           = 1;
static constexpr size_t PIPE_SHM_SIZE   = 512 * 1024;

static constexpr cycles_t FFT_TIME      = 17000 / 4;
static constexpr cycles_t EQ_TIME       = 6000;         // TODO

class MemBackedPipe {
public:
    explicit MemBackedPipe(size_t size)
        : _mem(MemGate::create_global(size, MemGate::RW)),
          _pipe(_mem, size) {
    }

    IndirectPipe &pipe() {
        return _pipe;
    }

private:
    MemGate _mem;
    IndirectPipe _pipe;
};

void chain_direct(File *in, size_t num) {
    VPEGroup *groups[num];
    VPE *vpes[2 + num * 3];
    StreamAccel *accels[1 + num * 3];
    MemBackedPipe *pipes[1 + num * 2];

    if(VERBOSE) Serial::get() << "Creating FFT VPE...\n";

    vpes[0] = new VPE("FFT", PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_FFT));
    if(Errors::last != Errors::NONE)
        exitmsg("Unable to create VPE for FFT");
    accels[0] = new StreamAccel(vpes[0], FFT_TIME);
    pipes[0] = new MemBackedPipe(PIPE_SHM_SIZE);

    // file -> FFT
    accels[0]->connect_input(static_cast<GenericFile*>(in));
    // FFT -> DISP
    File *wr = VPE::self().fds()->get(pipes[0]->pipe().writer_fd());
    accels[0]->connect_output(static_cast<GenericFile*>(wr));

    // create dispatcher first to reserve PE
    size_t didx = ARRAY_SIZE(vpes) - 1;
    vpes[didx] = new VPE("DISP");

    for(size_t j = 0; j < num; ++j) {
        groups[j] = new VPEGroup();

        // pipe from DISP -> EQ
        pipes[1 + j * 2 + 0] = new MemBackedPipe(PIPE_SHM_SIZE);
        // pipe from IFFT -> APP
        pipes[1 + j * 2 + 1] = new MemBackedPipe(PIPE_SHM_SIZE);

        // create accel VPEs
        const char *names[] = {"EQ", "IFFT"};
        const cycles_t times[] = {EQ_TIME, FFT_TIME};
        for(size_t i = 0; i < 2; ++i) {
            size_t idx = 1 + j * 3 + i;

            if(VERBOSE) Serial::get() << "Creating VPE " << names[i] << " for user " << j << "\n";

            vpes[idx] = new VPE(names[i], PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_FFT),
                                nullptr, VPE::MUXABLE, groups[j]);
            if(Errors::last != Errors::NONE)
                exitmsg("Unable to create VPE for " << names[i]);
            accels[idx] = new StreamAccel(vpes[idx], times[i]);
        }

        if(VERBOSE) Serial::get() << "Starting application...\n";

        // application
        size_t aidx = 1 + j * 3 + 2;
        vpes[aidx] = new VPE("APP", VPE::self().pe(), nullptr, VPE::MUXABLE, nullptr);
        File *ard = VPE::self().fds()->get(pipes[1 + j * 2 + 1]->pipe().reader_fd());
        vpes[aidx]->fds()->set(STDIN_FD, ard);
        vpes[aidx]->fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
        vpes[aidx]->obtain_fds();
        vpes[aidx]->run([] {
            alignas(64) char buffer[8192];
            cout << "Hello from application\n";
            File *in = VPE::self().fds()->get(STDIN_FD);
            while(1) {
                ssize_t amount = in->read(buffer, sizeof(buffer));
                cout << "Got " << amount << " bytes\n";
                if(amount <= 0)
                    break;
            }
            return 0;
        });
        pipes[1 + j * 2 + 1]->pipe().close_reader();

        if(VERBOSE) Serial::get() << "Connecting input and output...\n";

        // connect input/output

        // DISP -> EQ
        File *rd = VPE::self().fds()->get(pipes[1 + j * 2 + 0]->pipe().reader_fd());
        accels[1 + j * 3 + 0]->connect_input(static_cast<GenericFile*>(rd));
        // EQ -> IFFT
        accels[1 + j * 3 + 0]->connect_output(accels[1 + j * 3 + 1]);
        accels[1 + j * 3 + 1]->connect_input(accels[1 + j * 3 + 0]);
        // IFFT -> APP
        File *wr = VPE::self().fds()->get(pipes[1 + j * 2 + 1]->pipe().writer_fd());
        accels[1 + j * 3 + 1]->connect_output(static_cast<GenericFile*>(wr));
    }

    if(VERBOSE) Serial::get() << "Starting accelerators...\n";

    cycles_t start = Time::start(0);

    bool running[ARRAY_SIZE(vpes)];
    for(size_t i = 0; i < ARRAY_SIZE(vpes) - 1; ++i) {
        running[i] = true;
        if(!vpes[i]->pe().is_programmable())
            vpes[i]->start();
    }

    if(VERBOSE) Serial::get() << "Starting dispatcher...\n";

    // dispatcher
    File *in_pipe = VPE::self().fds()->get(pipes[0]->pipe().reader_fd());
    vpes[didx]->fds()->set(STDIN_FD, in_pipe);
    vpes[didx]->fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
    for(size_t i = 0; i < num; ++i)
        vpes[didx]->fds()->set(3 + i, VPE::self().fds()->get(pipes[1 + i * 2]->pipe().writer_fd()));
    vpes[didx]->obtain_fds();
    vpes[didx]->run([num] {
        alignas(64) char buffer[8192];
        File *in = VPE::self().fds()->get(STDIN_FD);
        for(size_t i = 0; ; ++i) {
            ssize_t amount = in->read(buffer, sizeof(buffer));
            if(amount <= 0) {
                if(VERBOSE) Serial::get() << "Received " << amount << ". Stopping.\n";
                break;
            }

            File *out_pipe = VPE::self().fds()->get(3 + (i % num));
            out_pipe->write(buffer, static_cast<size_t>(amount));
        }
        return 0;
    });
    running[didx] = true;

    if(VERBOSE) Serial::get() << "Closing pipe ends...\n";

    // close our ends
    pipes[0]->pipe().close_reader();
    for(size_t i = 0; i < num; ++i)
        pipes[1 + i * 2]->pipe().close_writer();

    if(VERBOSE) Serial::get() << "Waiting for VPEs...\n";

    // wait for their completion
    for(size_t rem = ARRAY_SIZE(vpes); rem > 0; --rem) {
        size_t count = 0;
        capsel_t sels[ARRAY_SIZE(vpes)];
        for(size_t i = 0; i < ARRAY_SIZE(vpes); ++i) {
            if(running[i])
                sels[count++] = vpes[i]->sel();
        }

        capsel_t vpe;
        int exitcode;
        if(Syscalls::get().vpewait(sels, rem, &vpe, &exitcode) != Errors::NONE)
            errmsg("Unable to wait for VPEs");
        else {
            for(size_t i = 0; i < ARRAY_SIZE(vpes); ++i) {
                if(running[i] && vpes[i]->sel() == vpe) {
                    if(VERBOSE) Serial::get() << "VPE" << i << " exited with exitcode " << exitcode << "\n";

                    if(exitcode != 0)
                        cerr << "VPE" << i << " terminated with exit code " << exitcode << "\n";
                    if(i == 0)
                        pipes[0]->pipe().close_writer();
                    else if(i != ARRAY_SIZE(vpes) - 1) {
                        size_t user = (i - 1) / 3;
                        size_t off = (i - 1) % 3;
                        if(off == 0)
                            pipes[1 + user * 2 + 0]->pipe().close_reader();
                        else if(off == 1)
                            pipes[1 + user * 2 + 1]->pipe().close_writer();
                    }
                    running[i] = false;
                    break;
                }
            }
        }
    }

    cycles_t end = Time::stop(0);
    Serial::get() << "Total time: " << (end - start) << " cycles\n";
}
