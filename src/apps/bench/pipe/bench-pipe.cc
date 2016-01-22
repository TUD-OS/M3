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

#include <m3/stream/Serial.h>
#include <m3/pipe/PipeWriter.h>
#include <m3/pipe/PipeReader.h>
#include <m3/util/Profile.h>
#include <m3/Log.h>

using namespace m3;

enum {
    BUF_SIZE    = 4 * 1024,
    MEM_SIZE    = BUF_SIZE * 128,
    TOTAL       = 2 * 1024 * 1024,
    COUNT       = TOTAL / BUF_SIZE,
};

alignas(DTU_PKG_SIZE) static char buffer[BUF_SIZE];

int main() {
    VPE writer("writer");
    Pipe pipe(VPE::self(), writer, MEM_SIZE);

    cycles_t start = Profile::start(0);

    writer.run([&pipe] {
        PipeWriter wr(pipe);
        for(size_t i = 0; i < COUNT; ++i)
            wr.write(buffer, sizeof(buffer));
        return 0;
    });

    {
        PipeReader rd(pipe);
        while(!rd.eof())
            rd.read(buffer, sizeof(buffer));
    }
    writer.wait();

    cycles_t end = Profile::stop(0);
    Serial::get() << "Transferred " << TOTAL << "b in " << BUF_SIZE << "b steps: " << (end - start) << " cycles\n";
    return 0;
}
