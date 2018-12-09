/*
 * Copyright (C) 2015-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Env.h>

#include <m3/session/ClientSession.h>
#include <m3/stream/Standard.h>
#include <m3/VPE.h>

#include <unistd.h>
#include <fcntl.h>

using namespace m3;

static void received_data(GateIStream &is) {
    unsigned sum = 0;
    const unsigned char *data = is.buffer();
    for(size_t i = 0; i < is.remaining(); ++i)
        sum += data[i];
    cout << env()->pe << ": received " << fmt(sum, "x") << "\n";
}

int main() {
    ClientSession qtest("queuetest");

    RecvGate rgate = RecvGate::create(nextlog2<4096>::val, nextlog2<512>::val);
    SendGate sgate = SendGate::create(&rgate, 0, SendGate::UNLIMITED);
    qtest.delegate_obj(sgate.sel());
    rgate.start(received_data);

    env()->workloop()->run();
    return 0;
}
