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

#include <base/Common.h>
#include <base/col/SList.h>
#include <base/util/Profile.h>
#include <base/KIF.h>
#include <base/Panic.h>

#include <m3/stream/Standard.h>
#include <m3/pipe/IndirectPipe.h>
#include <m3/Syscalls.h>

#include "../cppbench.h"

using namespace m3;

const size_t DATA_SIZE  = 2 * 1024 * 1024;
const size_t BUF_SIZE   = 8 * 1024;

alignas(64) static char buf[BUF_SIZE];

NOINLINE void child_to_parent() {
    Profile pr(2, 1);

    auto res = pr.run_with_id([] {
        MemGate mgate = MemGate::create_global(0x10000, MemGate::RW);
        IndirectPipe pipe(mgate, 0x10000);

        VPE vpe("writer");
        vpe.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe.writer_fd()));
        vpe.obtain_fds();

        vpe.run([] {
            auto output = VPE::self().fds()->get(STDOUT_FD);
            auto rem = DATA_SIZE;
            while(rem > 0) {
                output->write(buf, sizeof(buf));
                rem -= sizeof(buf);
            }
            return 0;
        });

        pipe.close_writer();

        auto input = VPE::self().fds()->get(pipe.reader_fd());
        while(input->read(buf, sizeof(buf)) > 0)
            ;

        pipe.close_reader();

        vpe.wait();
    }, 0x60);

    cout << "c->p: " << (DATA_SIZE / 1024) << " KiB transfer with "
         << (BUF_SIZE / 1024) << " KiB buf: " << res << "\n";
}

NOINLINE void parent_to_child() {
    Profile pr(2, 1);

    auto res = pr.run_with_id([] {
        MemGate mgate = MemGate::create_global(0x10000, MemGate::RW);
        IndirectPipe pipe(mgate, 0x10000);

        VPE vpe("writer");
        vpe.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
        vpe.obtain_fds();

        vpe.run([] {
            auto input = VPE::self().fds()->get(STDIN_FD);
            while(input->read(buf, sizeof(buf)) > 0)
                ;
            return 0;
        });

        pipe.close_reader();

        auto output = VPE::self().fds()->get(pipe.writer_fd());
        auto rem = DATA_SIZE;
        while(rem > 0) {
            output->write(buf, sizeof(buf));
            rem -= sizeof(buf);
        }

        pipe.close_writer();

        vpe.wait();
    }, 0x60);

    cout << "p->c: " << (DATA_SIZE / 1024) << " KiB transfer with "
         << (BUF_SIZE / 1024) << " KiB buf: " << res << "\n";
}

void bpipe() {
    RUN_BENCH(child_to_parent);
    RUN_BENCH(parent_to_child);
}
