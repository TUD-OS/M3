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

#include <base/Common.h>
#include <base/util/Profile.h>
#include <base/PEDesc.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>

#include <accel/stream/Stream.h>

using namespace m3;
using namespace accel;

static const int WARMUP    = 1;
static const int REPEATS   = 2;

template<bool AUTO>
static void execute(Stream &str, const char *in, const char *out, size_t bufsize) {
    fd_t infd = VFS::open(in, FILE_R);
    if(infd == FileTable::INVALID)
        exitmsg("Unable to open " << in);
    fd_t outfd = VFS::open(out, FILE_W | FILE_TRUNC | FILE_CREATE);
    if(outfd == FileTable::INVALID)
        exitmsg("Unable to open " << out << " for writing");

    if(AUTO)
        str.execute(VPE::self().fds()->get(infd), VPE::self().fds()->get(outfd), bufsize);
    else
        str.execute_slow(VPE::self().fds()->get(infd), VPE::self().fds()->get(outfd), bufsize);
    VFS::close(outfd);
    VFS::close(infd);
}

template<bool AUTO, uint NAME>
static void bench(Stream &str, const char *in, const char *out, size_t bufsize) {
    for(int j = 0; j < WARMUP; ++j)
        execute<AUTO>(str, in, out, bufsize);

    for(int j = 0; j < REPEATS; ++j) {
        Profile::start(NAME);
        execute<AUTO>(str, in, out, bufsize);
        Profile::stop(NAME);
    }
}

int main(int argc, char **argv) {
    if(argc < 3)
        exitmsg("Usage: " << argv[0] << " <in> <out>");

    if(VFS::mount("/", "m3fs") != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount filesystem\n");
    }

    Stream accel(PEISA::ACCEL_FFT);
    const size_t bufsize = 4096;
    bench<false, 0x1234>(accel, argv[1], argv[2], bufsize);
    bench<true,  0x1235>(accel, argv[1], argv[2], bufsize);
    return 0;
}
