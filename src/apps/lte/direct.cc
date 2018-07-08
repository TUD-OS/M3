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

static constexpr int VERBOSE            = 1;

static constexpr cycles_t FFT_TIME      = 17000 / 4;
static constexpr cycles_t EQ_TIME       = 6000;         // TODO

class MemBackedPipe {
public:
    explicit MemBackedPipe(MemGate &base, size_t off, size_t size)
        : _mem(base.derive(off, size, MemGate::RW)),
          _pipe("pipe", _mem, size),
          _rdfd(),
          _wrfd() {
    }
    ~MemBackedPipe() {
        close_reader();
        close_writer();
    }

    void create_channel(bool read, epid_t mep = EP_COUNT, size_t memoff = 0) {
        KIF::ExchangeArgs args;
        args.count = 1;
        args.vals[0] = read;
        KIF::CapRngDesc desc = _pipe.obtain(2, &args);
        fd_t fd = VPE::self().fds()->alloc(
            new GenericFile(read ? FILE_R : FILE_W, desc.start(), 0, mep, nullptr, memoff));
        if(read)
            _rdfd = fd;
        else
            _wrfd = fd;
    }

    fd_t reader_fd() const {
        return _rdfd;
    }
    fd_t writer_fd() const {
        return _wrfd;
    }

    void close_reader() {
        delete VPE::self().fds()->free(_rdfd);
    }
    void close_writer() {
        delete VPE::self().fds()->free(_wrfd);
    }

    MemGate _mem;
    Pipe _pipe;
    fd_t _rdfd;
    fd_t _wrfd;
};

void chain_direct(File *in, size_t pipesize, size_t num) {
    VPEGroup *groups[num];
    VPE *vpes[2 + num * 3];
    StreamAccel *accels[1 + num * 3];
    MemBackedPipe *pipes[1 + num * 2];

    MemGate basemem = MemGate::create_global(pipesize * (1 + num), MemGate::RW);
    basemem.activate_for(VPE::self(), VPE::self().alloc_ep());

    if(VERBOSE) Serial::get() << "Creating FFT VPE...\n";

    vpes[0] = new VPE("FFT", PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_FFT));
    if(Errors::last != Errors::NONE)
        exitmsg("Unable to create VPE for FFT");
    accels[0] = new StreamAccel(vpes[0], FFT_TIME);
    pipes[0] = new MemBackedPipe(basemem, 0, pipesize);
    pipes[0]->create_channel(false);
    pipes[0]->create_channel(true, basemem.ep(), 0);

    // file -> FFT
    accels[0]->connect_input(static_cast<GenericFile*>(in));
    // FFT -> DISP
    File *wr = VPE::self().fds()->get(pipes[0]->writer_fd());
    accels[0]->connect_output(static_cast<GenericFile*>(wr));

    // create dispatcher first to reserve PE
    size_t didx = ARRAY_SIZE(vpes) - 1;
    vpes[didx] = new VPE("DISP");
    basemem.activate_for(*vpes[didx], vpes[didx]->alloc_ep());

    for(size_t j = 0; j < num; ++j) {
        groups[j] = new VPEGroup();

        // pipe from DISP -> EQ
        size_t pipeidx = 1 + j * 2;
        pipes[pipeidx + 0] = new MemBackedPipe(basemem, pipesize + j * pipesize, pipesize);
        pipes[pipeidx + 0]->create_channel(false, basemem.ep(), pipesize + j * pipesize);
        pipes[pipeidx + 0]->create_channel(true);
        // pipe from IFFT -> APP
        pipes[pipeidx + 1] = new MemBackedPipe(basemem, pipesize + j * pipesize, pipesize);
        pipes[pipeidx + 1]->create_channel(false);
        pipes[pipeidx + 1]->create_channel(true);

        // create accel VPEs
        size_t aidx = 1 + j * 3;
        const char *names[] = {"EQ", "IFFT"};
        const cycles_t times[] = {EQ_TIME, FFT_TIME};
        for(size_t i = 0; i < 2; ++i) {
            if(VERBOSE) Serial::get() << "Creating VPE " << names[i] << " for user " << j << "\n";

            vpes[aidx + i] = new VPE(names[i], PEDesc(PEType::COMP_IMEM, PEISA::ACCEL_FFT),
                                nullptr, VPE::MUXABLE, groups[j]);
            if(Errors::last != Errors::NONE)
                exitmsg("Unable to create VPE for " << names[i]);
            accels[aidx + i] = new StreamAccel(vpes[aidx + i], times[i]);
        }

        if(VERBOSE) Serial::get() << "Starting application...\n";

        // application
        vpes[aidx + 2] = new VPE("APP", VPE::self().pe(), nullptr, VPE::MUXABLE, nullptr);
        File *ard = VPE::self().fds()->get(pipes[pipeidx + 1]->reader_fd());
        vpes[aidx + 2]->fds()->set(STDIN_FD, ard);
        vpes[aidx + 2]->fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
        vpes[aidx + 2]->obtain_fds();
        vpes[aidx + 2]->run([j] {
            alignas(64) static cycles_t buffer[8192 / sizeof(cycles_t)];
            cout << "Hello from user " << j << "\n";
            const cycles_t cycles_per_usec = DTU::get().clock() / 1000000;
            File *in = VPE::self().fds()->get(STDIN_FD);
            while(1) {
                ssize_t amount = in->read(buffer, sizeof(buffer));
                cycles_t start = buffer[0];
                cycles_t now = DTU::get().tsc();
                cout << "[user" << j << "] Got " << amount << " bytes (delay=" << ((now - start) / cycles_per_usec) << "us)\n";
                if(amount <= 0)
                    break;
            }
            return 0;
        });
        pipes[pipeidx + 1]->close_reader();

        if(VERBOSE) Serial::get() << "Connecting input and output...\n";

        // connect input/output

        // DISP -> EQ
        File *rd = VPE::self().fds()->get(pipes[pipeidx + 0]->reader_fd());
        accels[aidx + 0]->connect_input(static_cast<GenericFile*>(rd));
        // EQ -> IFFT
        accels[aidx + 0]->connect_output(accels[aidx + 1]);
        accels[aidx + 1]->connect_input(accels[aidx + 0]);
        // IFFT -> APP
        File *wr = VPE::self().fds()->get(pipes[pipeidx + 1]->writer_fd());
        accels[aidx + 1]->connect_output(static_cast<GenericFile*>(wr));
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
    // TODO this doesn't work with exec yet, because we do not properly serialize GenericFile!
    vpes[didx]->fds()->set(STDIN_FD, VPE::self().fds()->get(pipes[0]->reader_fd()));
    vpes[didx]->fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
    for(size_t i = 0; i < num; ++i)
        vpes[didx]->fds()->set(3 + i, VPE::self().fds()->get(pipes[1 + i * 2]->writer_fd()));
    vpes[didx]->obtain_fds();
    vpes[didx]->run([num] {
        const cycles_t cycles_per_usec = DTU::get().clock() / 1000000;
        const cycles_t sleep_time = 100 /* us */ * cycles_per_usec;
        const cycles_t max_delay  = 1000 /* us */ * cycles_per_usec;

        RecvGate rgate = RecvGate::create(nextlog2<32 * 64>::val, nextlog2<64>::val);
        rgate.activate();

        GenericFile *in = static_cast<GenericFile*>(VPE::self().fds()->get(STDIN_FD));
        in->sgate().reply_gate(&rgate);

        bool sent_in_req = false;
        bool sent_out_req[num];
        cycles_t last_push[num];
        for(size_t i = 0; i < ARRAY_SIZE(last_push); ++i) {
            GenericFile *out_pipe = static_cast<GenericFile*>(VPE::self().fds()->get(3 + i));
            out_pipe->sgate().reply_gate(&rgate);
            sent_out_req[i] = false;
            last_push[i] = 0;
        }

        size_t no = 0;
        while(1) {
            cycles_t now = DTU::get().tsc();

            // have we got a response to a next-input request?
            const DTU::Message *msg = rgate.fetch();
            if(msg) {
                if(VERBOSE > 1) Serial::get() << "[" << (now / cycles_per_usec) << "] Received response from " << msg->label << "\n";
                GateIStream is(rgate, msg);
                if(msg->label == 1) {
                    if(in->received_next_resp(is) == 0) {
                        if(VERBOSE) Serial::get() << "Received EOF. Stopping.\n";
                        break;
                    }
                    sent_in_req = false;
                }
                else {
                    size_t user = msg->label - 2;
                    GenericFile *out_pipe = static_cast<GenericFile*>(VPE::self().fds()->get(3 + user));
                    out_pipe->received_next_resp(is);
                    sent_out_req[user] = false;
                }
            }

            alignas(64) static cycles_t buffer[8192 / sizeof(cycles_t)];
            while(in->has_data()) {
                size_t user = no % num;
                GenericFile *out_pipe = static_cast<GenericFile*>(VPE::self().fds()->get(3 + user));
                if(out_pipe->has_data()) {
                    ssize_t amount = in->read(buffer, sizeof(buffer));
                    assert(amount > 0);

                    buffer[0] = DTU::get().tsc();

                    // push it into the user pipe
                    if(VERBOSE > 1) Serial::get() << "[" << (now / cycles_per_usec) << "] Pushing to user " << user << "\n";
                    out_pipe->write(buffer, static_cast<size_t>(amount));
                    if(last_push[user] == 0)
                        last_push[user] = now;
                    no += 1;
                }
                else {
                    if(!sent_out_req[user]) {
                        if(VERBOSE > 1) Serial::get() << "[" << (now / cycles_per_usec) << "] Sending output request for " << user << "\n";
                        out_pipe->send_next_output(2 + user);
                        sent_out_req[user] = true;
                    }
                    break;
                }
            }

            // otherwise, request new data
            if(!in->has_data() && !sent_in_req) {
                if(VERBOSE > 1) Serial::get() << "[" << (now / cycles_per_usec) << "] Sending input request\n";
                in->send_next_input(1);
                sent_in_req = true;
            }

            // flush the pipes for which the max latency is reached
            // for(size_t i = 0; i < ARRAY_SIZE(last_push); ++i) {
            //     if(last_push[i] > 0 && now - last_push[i] > max_delay) {
            //         if(VERBOSE > 1) Serial::get() << "[" << (now / cycles_per_usec) << "] Committing to user " << i << "\n";
            //         File *out_pipe = VPE::self().fds()->get(3 + i);
            //         out_pipe->flush();
            //         last_push[i] = 0;
            //     }
            // }

            // wait for some time or the next message
            DTU::get().try_sleep(true, sleep_time);
        }

        VFS::close(0);
        for(size_t i = 0; i < num; ++i)
            VFS::close(3 + i);
        return 0;
    });
    running[didx] = true;

    if(VERBOSE) Serial::get() << "Closing pipe ends...\n";

    // close our ends
    pipes[0]->close_reader();
    for(size_t i = 0; i < num; ++i)
        pipes[1 + i * 2]->close_writer();

    if(VERBOSE) Serial::get() << "Waiting for VPEs...\n";

    // wait for their completion
    for(size_t rem = ARRAY_SIZE(vpes); rem > 0; --rem) {
        size_t count = 0;
        capsel_t sels[ARRAY_SIZE(vpes)];
        for(size_t i = 0; i < ARRAY_SIZE(vpes); ++i) {
            if(running[i])
                sels[count++] = vpes[i]->sel();
        }

        if(VERBOSE) Serial::get() << "Waiting for " << rem << " VPEs...\n";

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
                        pipes[0]->close_writer();
                    else if(i != ARRAY_SIZE(vpes) - 1) {
                        size_t user = (i - 1) / 3;
                        size_t off = (i - 1) % 3;
                        if(off == 0)
                            pipes[1 + user * 2 + 0]->close_reader();
                        else if(off == 1)
                            pipes[1 + user * 2 + 1]->close_writer();
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
