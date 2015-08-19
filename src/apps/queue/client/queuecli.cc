/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/cap/Session.h>
#include <m3/cap/VPE.h>
#include <m3/Syscalls.h>
#include <m3/GateStream.h>
#include <m3/WorkLoop.h>
#include <m3/Log.h>

#include <unistd.h>
#include <fcntl.h>

using namespace m3;

static void received_data(RecvGate &gate, Subscriber<RecvGate&> *) {
    GateIStream is(gate);
    unsigned sum = 0;
    const unsigned char *data = is.buffer();
    for(size_t i = 0; i < is.remaining(); ++i)
        sum += data[i];
    LOG(DEF, "Received " << fmt(sum, "x"));
}

int main() {
    Session qtest("queuetest");

    RecvBuf rcvbuf = RecvBuf::create(VPE::self().alloc_chan(),
            nextlog2<4096>::val, nextlog2<512>::val, 0);
    RecvGate rgate = RecvGate::create(&rcvbuf);
    SendGate sgate = SendGate::create(SendGate::UNLIMITED, &rgate);
    qtest.delegate(CapRngDesc(sgate.sel()));
    rgate.subscribe(received_data);

    WorkLoop::get().run();
    return 0;
}
