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

#include <m3/stream/Standard.h>

#include "swfilter.h"

using namespace m3;

int main() {
    File *in = VPE::self().fds()->get(STDIN_FD);
    File *out = VPE::self().fds()->get(STDOUT_FD);

    ulong *buf = new ulong[SWFIL_BUF_SIZE / sizeof(ulong)];
    ssize_t amount;
    while((amount = in->read(buf, SWFIL_BUF_SIZE)) > 0) {
        size_t num = static_cast<size_t>(amount) / sizeof(ulong);
        for(size_t i = 0; i < num; i += 4) {
            buf[i] = (buf[i] > 100) ? 0 : buf[i];
            buf[i + 1] = (buf[i + 1] > 100) ? 0 : buf[i + 1];
            buf[i + 2] = (buf[i + 2] > 100) ? 0 : buf[i + 2];
            buf[i + 3] = (buf[i + 3] > 100) ? 0 : buf[i + 3];
        }

        if(out->write(buf, static_cast<size_t>(amount)) < 0)
            exitmsg("Unable to write to accel pipe");
    }
    if(amount < 0)
        exitmsg("Unable to read from accel pipe");
    return 0;
}
