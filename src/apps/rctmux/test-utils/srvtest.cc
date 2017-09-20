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
#include <base/util/Profile.h>
#include <base/Panic.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

#define VERBOSE     0

static const int WARMUP = 10;
static const int REPEAT = 10;

static void start(VPE &v, int argc, const char **argv) {
    v.mountspace(*VPE::self().mountspace());
    v.obtain_mountspace();
    Errors::Code res = v.exec(argc, argv);
    if(res != Errors::NONE)
        PANIC("Cannot execute " << argv[0] << ": " << Errors::to_string(res));
}

int main(int argc, char **argv) {
    bool muxable = argc > 1 && strcmp(argv[1], "1") == 0;

    if(VERBOSE) cout << "Mounting filesystem...\n";
    if(VFS::mount("/", "m3fs") != Errors::NONE)
        PANIC("Cannot mount root fs");

    if(VERBOSE) cout << "Creating VPEs...\n";

    const char *args1[] = {"/bin/rctmux-util-service", "srv1"};
    VPE s1(args1[0], VPE::self().pe(), "pager", muxable);
    if(Errors::last != Errors::NONE)
        exitmsg("Unable to create VPE");

    const char *args2[] = {"/bin/rctmux-util-service", "srv2"};
    VPE s2(args2[0], VPE::self().pe(), "pager", muxable);
    if(Errors::last != Errors::NONE)
        exitmsg("Unable to create VPE");

    if(VERBOSE) cout << "Starting VPEs...\n";

    start(s1, ARRAY_SIZE(args1), args1);
    start(s2, ARRAY_SIZE(args2), args2);

    enum TestOp {
        TEST
    };

    if(VERBOSE) cout << "Starting session creation...\n";

    Session *sess[2] = {nullptr, nullptr};
    SendGate *sgate[2] = {nullptr, nullptr};
    const char *name[2] = {nullptr, nullptr};

    for(int i = 0; i < 2; ++i) {
        name[i] = i == 0 ? "srv1" : "srv2";

        // the kernel does not block us atm until the service is available
        // so try to connect until it's available
        while(sess[i] == nullptr) {
            sess[i] = new Session(name[i]);
            if(sess[i]->is_connected())
                break;

            for(volatile int x = 0; x < 10000; ++x)
                ;
            delete sess[i];
            sess[i] = nullptr;
        }

        sgate[i] = new SendGate(SendGate::bind(sess[i]->obtain(1).start()));
    }

    if(VERBOSE) cout << "Starting test...\n";

    for(int i = 0; i < WARMUP; ++i) {
        int no = i % 2;
        GateIStream reply = send_receive_vmsg(*sgate[no], TEST);

        int res;
        reply >> res;
        cout << "Got " << res << " from " << name[no] << "\n";
    }

    for(int i = 0; i < REPEAT; ++i) {
        int no = i % 2;

        cycles_t start = Profile::start(0x1234);
        GateIStream reply = send_receive_vmsg(*sgate[no], TEST);
        cycles_t end = Profile::stop(0x1234);

        int res;
        reply >> res;
        cout << "Got " << res << " from " << name[no] << " (" << (end - start) << " cycles)\n";
    }

    if(VERBOSE) cout << "Test finished.\n";

    for(int i = 0; i < 2; ++i) {
        delete sgate[i];
        delete sess[i];
    }

    return 0;
}
