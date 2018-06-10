/**
 * Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
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
#include <m3/vfs/Dir.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

#define VERBOSE         1

struct App {
    explicit App(int argc, const char **argv, uint flags)
        : argc(argc),
          argv(argv),
          vpe(argv[0], VPE::self().pe(), "pager", flags),
          rgate(RecvGate::create_for(vpe, 6, 6)),
          sgate(SendGate::create(&rgate)) {
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to create VPE");
        rgate.activate();
        vpe.delegate_obj(rgate.sel());
    }

    int argc;
    const char **argv;
    VPE vpe;
    RecvGate rgate;
    SendGate sgate;
};

int main(int argc, char **argv) {
    if(argc != 6) {
        cerr << "Usage: " << argv[0] << " 1|0 <repeats> <instances> <servers> <fssize>\n";
        return 1;
    }

    if(VERBOSE) cout << "Mounting filesystem...\n";

    if(VFS::mount("/", "m3fs") != Errors::NONE)
        PANIC("Cannot mount root fs");

    bool muxed = strcmp(argv[1], "1") == 0;
    UNUSED int repeats = IStringStream::read_from<int>(argv[2]);
    size_t instances = IStringStream::read_from<size_t>(argv[3]);
    size_t servers = IStringStream::read_from<size_t>(argv[4]);
    size_t fssize = IStringStream::read_from<size_t>(argv[5]);
    App *apps[instances];

    RemoteServer *srv[servers];
    VPE *srvvpes[servers];
    char srvnames[servers][16];

    if(VERBOSE) cout << "Creating application VPEs...\n";

    constexpr size_t ARG_COUNT = 8;
    for(size_t i = 0; i < instances; ++i) {
        const char **args = new const char *[ARG_COUNT];
        args[0] = "/bin/fstrace-m3fs";

        apps[i] = new App(ARG_COUNT, args, muxed ? (VPE::MUXABLE | VPE::PINNED) : 0);
    }

    if(VERBOSE) cout << "Creating servers...\n";

    for(size_t i = 0; i < servers; ++i) {
        srvvpes[i] = new VPE("m3fs", VPE::self().pe(), "pager", muxed ? VPE::MUXABLE : 0);
        OStringStream name(srvnames[i], sizeof(srvnames[i]));
        name << "m3fs" << i;
        srv[i] = new RemoteServer(*srvvpes[i], name.str());

        String m3fsarg = srv[i]->sel_arg();
        OStringStream fs_off_str(new char[16], 16);
        fs_off_str << (fssize * i);
        OStringStream fs_size_str(new char[16], 16);
        fs_size_str << fssize;
        const char *m3fs_args[] = {
            "/bin/m3fs", "-n", srvnames[i], "-s", m3fsarg.c_str(),
            "-o", fs_off_str.str(), fs_size_str.str()
        };
        if(VERBOSE) {
            cout << "Creating ";
            for(size_t x = 0; x < ARRAY_SIZE(m3fs_args); ++x)
                cout << m3fs_args[x] << " ";
            cout << "\n";
        }
        Errors::Code res = srvvpes[i]->exec(ARRAY_SIZE(m3fs_args), m3fs_args);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << m3fs_args[0] << ": " << Errors::to_string(res));
    }

    if(VERBOSE) cout << "Starting VPEs...\n";

    for(size_t i = 0; i < instances; ++i) {
        OStringStream tmpdir(new char[16], 16);
        tmpdir << "/tmp/" << i << "/";
        const char **args = apps[i]->argv;
        args[1] = "-p";
        args[2] = tmpdir.str();
        args[3] = "-w";
        args[4] = "-f";
        args[5] = srvnames[i % servers];
        args[6] = "-g";

        OStringStream rgatesel(new char[11], 11);
        rgatesel << apps[i]->rgate.sel() << " " << apps[i]->rgate.ep();
        args[7] = rgatesel.str();

        if(VERBOSE) {
            cout << "Starting ";
            for(size_t x = 0; x < ARG_COUNT; ++x)
                cout << args[x] << " ";
            cout << "\n";
        }

        Errors::Code res = apps[i]->vpe.exec(apps[i]->argc, apps[i]->argv);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << apps[i]->argv[0] << ": " << Errors::to_string(res));
    }

    if(VERBOSE) cout << "Signaling VPEs...\n";

    for(size_t i = 0; i < instances; ++i)
        send_receive_vmsg(apps[i]->sgate, 1);
    for(size_t i = 0; i < instances; ++i)
        send_vmsg(apps[i]->sgate, 1);

    cycles_t start = Time::start(0x1234);

    if(VERBOSE) cout << "Waiting for VPEs...\n";

    for(size_t i = 0; i < instances; ++i) {
        int res = apps[i]->vpe.wait();
        if(VERBOSE) cout << apps[i]->argv[0] << " exited with " << res << "\n";
    }

    cycles_t end = Time::stop(0x1234);
    cout << "Time: " << (end - start) << "\n";

    if(VERBOSE) cout << "Deleting VPEs...\n";

    for(size_t i = 0; i < instances; ++i)
        delete apps[i];

    if(VERBOSE) cout << "Shutting down servers...\n";

    for(size_t i = 0; i < servers; ++i) {
        srv[i]->request_shutdown();
        int res = srvvpes[i]->wait();
        if(VERBOSE) cout << srvnames[i] << " exited with " << res << "\n";
    }
    for(size_t i = 0; i < servers; ++i) {
        delete srvvpes[i];
        delete srv[i];
    }

    if(VERBOSE) cout << "Done\n";
    return 0;
}
