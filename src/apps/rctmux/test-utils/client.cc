/**
* Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
* Economic rights: Technische Universität Dresden (Germany)
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

#include <base/Common.h>
#include <base/stream/IStringStream.h>
#include <base/util/Time.h>

#include <m3/com/SendGate.h>
#include <m3/com/GateStream.h>
#include <m3/session/ClientSession.h>
#include <m3/stream/Standard.h>

using namespace m3;

enum TestOp {
    TEST
};

static const int WARMUP = 10;
static const int REPEAT = 10;

int main(int argc, char **argv) {
    int mode = argc > 1 ? IStringStream::read_from<int>(argv[1]) : 0;

    ClientSession *sess[2] = {nullptr, nullptr};
    SendGate *sgate[2] = {nullptr, nullptr};
    const char *name[2] = {nullptr, nullptr};

    for(int i = 0; i < (mode == 2 ? 1 : 2); ++i) {
        name[i] = i == 0 ? "srv1" : "srv2";
        sess[i] = new ClientSession(name[i]);
        sgate[i] = new SendGate(SendGate::bind(sess[i]->obtain(1).start()));
    }

    for(int i = 0; i < WARMUP; ++i) {
        int no = mode == 2 ? 0 : i % 2;
        GateIStream reply = send_receive_vmsg(*sgate[no], TEST);

        int res;
        reply >> res;
    }

    cycles_t total = 0;
    for(int i = 0; i < REPEAT; ++i) {
        int no = mode == 2 ? 0 : i % 2;

        cycles_t start = Time::start(0x1234);
        GateIStream reply = send_receive_vmsg(*sgate[no], TEST);
        cycles_t end = Time::stop(0x1234);

        int res;
        reply >> res;
        total += end - start;
    }

    cout << "Time: " << (total / REPEAT) << "\n";

    for(int i = 0; i < 2; ++i) {
        delete sgate[i];
        delete sess[i];
    }
    return 0;
}
