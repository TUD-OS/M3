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
#include <base/Panic.h>

#include <m3/server/RemoteServer.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

#define VERBOSE     0

static void start(VPE &v, int argc, const char **argv) {
    v.mounts(*VPE::self().mounts());
    v.obtain_mounts();
    Errors::Code res = v.exec(argc, argv);
    if(res != Errors::NONE)
        PANIC("Cannot execute " << argv[0] << ": " << Errors::to_string(res));
}

int main(int argc, char **argv) {
    int mode = argc > 1 ? IStringStream::read_from<int>(argv[1]) : 0;

    if(VERBOSE) cout << "Mounting filesystem...\n";

    if(VFS::mount("/", "m3fs") != Errors::NONE)
        PANIC("Cannot mount root fs");

    {
        if(VERBOSE) cout << "Creating VPEs...\n";

        VPE c1("client", VPE::self().pe(), "pager", mode == 2);
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to create VPE");

        VPE s1("service1", VPE::self().pe(), "pager", mode >= 1);
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to create VPE");

        VPE s2("service2", VPE::self().pe(), "pager", mode >= 1);
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to create VPE");

        if(VERBOSE) cout << "Creating services...\n";

        RemoteServer srv1(s1, "srv1");
        RemoteServer srv2(s2, "srv2");

        if(VERBOSE) cout << "Starting VPEs...\n";

        String srv1arg = srv1.sel_arg();
        String srv2arg = srv2.sel_arg();
        const char *args1[] = {"/bin/rctmux-util-client", argc > 1 ? argv[1] : "0"};
        const char *args2[] = {"/bin/rctmux-util-service", srv1arg.c_str()};
        const char *args3[] = {"/bin/rctmux-util-service", srv2arg.c_str()};
        start(s1, ARRAY_SIZE(args2), args2);
        start(s2, ARRAY_SIZE(args3), args3);
        start(c1, ARRAY_SIZE(args1), args1);

        if(VERBOSE) cout << "Waiting for client VPE...\n";

        int exit1 = c1.wait();
        if(VERBOSE) cout << "Client exited with " << exit1 << "\n";

        if(VERBOSE) cout << "Requesting shutdown\n";

        srv1.request_shutdown();
        srv2.request_shutdown();

        if(VERBOSE) cout << "Waiting for service VPEs\n";

        int exit2 = s1.wait();
        if(VERBOSE) cout << "Service 1 exited with " << exit2 << "\n";
        int exit3 = s2.wait();
        if(VERBOSE) cout << "Service 2 exited with " << exit3 << "\n";
    }

    return 0;
}
