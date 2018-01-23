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

#include <base/util/Time.h>

#include <m3/stream/Standard.h>
#include <m3/pipe/DirectPipe.h>
#include <m3/pipe/IndirectPipe.h>

using namespace m3;

enum {
    BUF_SIZE    = 4 * 1024,
    MEM_SIZE    = BUF_SIZE * 128,
    TOTAL       = 2 * 1024 * 1024,
    COUNT       = TOTAL / BUF_SIZE,
};

alignas(64) static char buffer[BUF_SIZE];

template<class PIPE>
static void child_to_parent(const char *name, VPE &writer, PIPE &pipe) {
    cycles_t start = Time::start(0);

    writer.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe.writer_fd()));
    writer.obtain_fds();

    writer.run([] {
        File *out = VPE::self().fds()->get(STDOUT_FD);
        for(size_t i = 0; i < COUNT; ++i)
            out->write(buffer, sizeof(buffer));
        return 0;
    });

    pipe.close_writer();

    File *in = VPE::self().fds()->get(pipe.reader_fd());
    while(in->read(buffer, sizeof(buffer)) > 0)
        ;

    pipe.close_reader();
    writer.wait();

    cycles_t end = Time::stop(0);
    cout << "[" << name << "] Transferred " << TOTAL << "b in " << BUF_SIZE << "b steps: ";
    cout << (end - start) << " cycles\n";
}

template<class PIPE>
static void parent_to_child(const char *name, VPE &reader, PIPE &pipe) {
    cycles_t start = Time::start(0);

    reader.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
    reader.obtain_fds();

    reader.run([] {
        File *in = VPE::self().fds()->get(STDIN_FD);
        while(in->read(buffer, sizeof(buffer)) > 0)
            ;
        return 0;
    });

    pipe.close_reader();

    File *out = VPE::self().fds()->get(pipe.writer_fd());
    for(size_t i = 0; i < COUNT; ++i)
        out->write(buffer, sizeof(buffer));

    pipe.close_writer();
    reader.wait();

    cycles_t end = Time::stop(0);
    cout << "[" << name << "] Transferred " << TOTAL << "b in " << BUF_SIZE << "b steps: ";
    cout << (end - start) << " cycles\n";
}

int main(int argc, char **argv) {
    bool direct = true;
    bool indirect = true;
    if(argc > 1) {
        direct = strcmp(argv[1], "direct") == 0;
        indirect = strcmp(argv[1], "indirect") == 0;
    }

    MemGate mem = MemGate::create_global(MEM_SIZE, MemGate::RW);

    if(direct) {
        {
            VPE writer("writer");
            DirectPipe pipe(VPE::self(), writer, mem, MEM_SIZE);
            child_to_parent("  dir:c->p", writer, pipe);
        }

        {
            VPE reader("reader");
            DirectPipe pipe(reader, VPE::self(), mem, MEM_SIZE);
            parent_to_child("  dir:p->c", reader, pipe);
        }
    }

    if(indirect) {
        {
            VPE writer("writer");
            IndirectPipe pipe(mem, MEM_SIZE);
            child_to_parent("indir:c->p", writer, pipe);
        }

        {
            VPE reader("reader");
            IndirectPipe pipe(mem, MEM_SIZE);
            parent_to_child("indir:p->c", reader, pipe);
        }
    }
    return 0;
}
