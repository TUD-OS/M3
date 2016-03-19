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
#include <base/util/Profile.h>
#include <base/Log.h>

#include <m3/pipe/PipeWriter.h>
#include <m3/pipe/PipeReader.h>
#include <m3/vfs/FileRef.h>

using namespace m3;

enum {
    BUF_SIZE    = 4 * 1024,
    MEM_SIZE    = BUF_SIZE * 128,
};

alignas(DTU_PKG_SIZE) static char buffer[BUF_SIZE];

NOINLINE void replace(char *buffer, long res, char c1, char c2) {
    long i;
    for(i = 0; i < res; ++i) {
        if(buffer[i] == c1)
            buffer[i] = c2;
    }
}

int main(int argc, char **argv) {
    if(argc < 5) {
        Serial::get() << "Usage: " << argv[0] << " <in> <out> <s> <r>\n";
        return 1;
    }

    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Mounting root-fs failed");

    cycles_t apptime = 0;
    cycles_t start = Profile::start(0);

    VPE writer("writer");
    Pipe pipe(VPE::self(), writer, MEM_SIZE);

    writer.delegate_mounts();
    writer.run([argv, &pipe] {
        FileRef input(argv[1], FILE_R);
        if(Errors::occurred())
            PANIC("open of " << argv[1] << " failed (" << Errors::last << ")");

        size_t res;
        PipeWriter wr(pipe);
        while((res = input->read(buffer, sizeof(buffer))) > 0)
            wr.write(buffer, res);
        return 0;
    });

    {
        FileRef output(argv[2], FILE_W | FILE_CREATE | FILE_TRUNC);
        if(Errors::occurred())
            PANIC("open of " << argv[2] << " failed (" << Errors::last << ")");

        PipeReader rd(pipe);

        char c1 = argv[3][0];
        char c2 = argv[4][0];

        size_t res;
        while((res = rd.read(buffer, sizeof(buffer))) > 0) {
            cycles_t cstart = Profile::start(0xaaaa);
            replace(buffer, res, c1, c2);
            cycles_t cend = Profile::stop(0xaaaa);
            apptime += cend - cstart;
            output->write(buffer, res);
        }
    }
    writer.wait();

    cycles_t end = Profile::stop(0);
    Serial::get() << "Total time: " << (end - start) << " cycles\n";
    Serial::get() << "App time: " << apptime << " cycles\n";
    return 0;
}
