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

#include <base/util/Profile.h>

#include <m3/stream/Standard.h>
#include <m3/pipe/Pipe.h>

using namespace m3;

enum {
    BUF_SIZE    = 4 * 1024,
    MEM_SIZE    = BUF_SIZE * 128,
    TOTAL       = 2 * 1024 * 1024,
    COUNT       = TOTAL / BUF_SIZE,
};

alignas(DTU_PKG_SIZE) static char buffer[BUF_SIZE];

int main() {
    cycles_t start = Profile::start(0);

    VPE writer("writer");
    Pipe pipe(VPE::self(), writer, MEM_SIZE);

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

    cycles_t end = Profile::stop(0);
    cout << "Transferred " << TOTAL << "b in " << BUF_SIZE << "b steps: " << (end - start) << " cycles\n";
    return 0;
}
