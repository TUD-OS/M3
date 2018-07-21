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
#include <base/stream/IStringStream.h>
#include <base/util/Time.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>

#include "imgproc.h"

using namespace m3;

const cycles_t ACCEL_TIMES[] = {
    5856 / 2,   // FFT
    1189 / 2,   // multiply
    5856 / 2,   // IFFT
};

int main(int argc, char **argv) {
    if(argc < 5)
        exitmsg("Usage: " << argv[0] << " <in> <mode> <num> <repeats>");

    if(VFS::mount("/", "m3fs") != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Unable to mount filesystem\n");
    }

    const char *in = argv[1];
    Mode mode = static_cast<Mode>(IStringStream::read_from<int>(argv[2]));
    size_t num = IStringStream::read_from<size_t>(argv[3]);
    int repeats = IStringStream::read_from<int>(argv[4]);

    for(int i = 0; i < repeats; ++i) {
        if(mode == Mode::INDIR)
            chain_indirect(in, num);
        else
            chain_direct(in, num, mode);
    }
    return 0;
}
