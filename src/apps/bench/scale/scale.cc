/**
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
    explicit App(size_t argc, const char **argv, const char *pager, uint flags)
        : argc(argc),
          argv(argv),
          vpe(argv[0], VPE::self().pe(), pager, flags),
          rgate(RecvGate::create_for(vpe, 6, 6)),
          sgate(SendGate::create(&rgate)) {
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to create VPE");
        rgate.activate();
        vpe.delegate_obj(rgate.sel());
    }

    size_t argc;
    const char **argv;
    VPE vpe;
    RecvGate rgate;
    SendGate sgate;
};

int main(int argc, char **argv) {
    if(argc != 8) {
        cerr << "Usage: " << argv[0] << " <name> <muxed> <loadgen> <repeats> <instances> <servers> <fssize>\n";
        return 1;
    }

    if(VERBOSE) cout << "Mounting filesystem...\n";

    if(VFS::mount("/", "m3fs") != Errors::NONE)
        PANIC("Cannot mount root fs");

    const char *name = argv[1];
    bool muxed = strcmp(argv[2], "1") == 0;
    bool loadgen = strcmp(argv[3], "1") == 0;
    int repeats = IStringStream::read_from<int>(argv[4]);
    size_t instances = IStringStream::read_from<size_t>(argv[5]);
    size_t servers = IStringStream::read_from<size_t>(argv[6]);
    size_t fssize = IStringStream::read_from<size_t>(argv[7]);
    App *apps[instances];

    RemoteServer *srv[1 + servers];
    VPE *srvvpes[1 + servers];
    char srvnames[1 + servers][16];

    if(VERBOSE) cout << "Creating pager...\n";

    {
        srvvpes[0] = new VPE("pager", VPE::self().pe(), "pager", muxed ? VPE::MUXABLE : 0);
        srv[0] = new RemoteServer(*srvvpes[0], "mypager");
        OStringStream pager_name(srvnames[0], sizeof(srvnames[0]));
        pager_name << "pager";

        String srv_arg = srv[0]->sel_arg();
        const char *args[] = {"/bin/pager", "-a", "16", "-f", "16", "-s", srv_arg.c_str()};
        Errors::Code res = srvvpes[0]->exec(ARRAY_SIZE(args), args);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << args[0] << ": " << Errors::to_string(res));
    }

    if(VERBOSE) cout << "Creating application VPEs...\n";

    const size_t ARG_COUNT = loadgen ? 11 : 9;
    for(size_t i = 0; i < instances; ++i) {
        const char **args = new const char *[ARG_COUNT];
        args[0] = "/bin/fstrace-m3fs";

        apps[i] = new App(ARG_COUNT, args, "mypager", muxed ? (VPE::MUXABLE | VPE::PINNED) : 0);
    }

    if(VERBOSE) cout << "Creating servers...\n";

    for(size_t i = 0; i < servers; ++i) {
        srvvpes[i + 1] = new VPE("m3fs", VPE::self().pe(), "pager", muxed ? VPE::MUXABLE : 0);
        OStringStream m3fs_name(srvnames[i + 1], sizeof(srvnames[i + 1]));
        m3fs_name << "m3fs" << i;
        srv[i + 1] = new RemoteServer(*srvvpes[i + 1], m3fs_name.str());

        String m3fsarg = srv[i + 1]->sel_arg();
        OStringStream fs_off_str(new char[16], 16);
        fs_off_str << (fssize * i);
        OStringStream fs_size_str(new char[16], 16);
        fs_size_str << fssize;
        const char *m3fs_args[] = {
            "/bin/m3fs",
            "-n", srvnames[i + 1],
            "-s", m3fsarg.c_str(),
            "-o", fs_off_str.str(),
            "-e", "512",
            "mem",
            fs_size_str.str()
        };
        if(VERBOSE) {
            cout << "Creating ";
            for(size_t x = 0; x < ARRAY_SIZE(m3fs_args); ++x)
                cout << m3fs_args[x] << " ";
            cout << "\n";
        }
        Errors::Code res = srvvpes[i + 1]->exec(ARRAY_SIZE(m3fs_args), m3fs_args);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << m3fs_args[0] << ": " << Errors::to_string(res));
    }

    if(VERBOSE) cout << "Starting VPEs...\n";

    for(size_t i = 0; i < instances; ++i) {
        OStringStream tmpdir(new char[16], 16);
        tmpdir << "/tmp/" << i << "/";
        const char **args = apps[i]->argv;
        if(repeats > 1) {
            args[1] = "-n";
            OStringStream num(new char[16], 16);
            num << repeats;
            args[2] = num.str();
        }
        else {
            args[1] = "-p";
            args[2] = tmpdir.str();
        }
        args[3] = "-w";
        args[4] = "-f";
        args[5] = srvnames[1 + (i % servers)];
        args[6] = "-g";

        OStringStream rgatesel(new char[11], 11);
        rgatesel << apps[i]->rgate.sel() << " " << apps[i]->rgate.ep();
        args[7] = rgatesel.str();
        if(loadgen) {
            args[8] = "-l";
            OStringStream loadgen(new char[16], 16);
            loadgen << "loadgen" << (i % 8);
            args[9] = loadgen.str();
            args[10] = name;
        }
        else
            args[8] = name;

        if(VERBOSE) {
            cout << "Starting ";
            for(size_t x = 0; x < ARG_COUNT; ++x)
                cout << args[x] << " ";
            cout << "\n";
        }

        Errors::Code res = apps[i]->vpe.exec(static_cast<int>(apps[i]->argc), apps[i]->argv);
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

    for(size_t i = 0; i < servers + 1; ++i) {
        srv[i]->request_shutdown();
        int res = srvvpes[i]->wait();
        if(VERBOSE) cout << srvnames[i] << " exited with " << res << "\n";
    }
    for(size_t i = 0; i < servers + 1; ++i) {
        delete srvvpes[i];
        delete srv[i];
    }

    if(VERBOSE) cout << "Done\n";
    return 0;
}
