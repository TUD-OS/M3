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
#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/Dir.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

#define VERBOSE         1

struct App {
    explicit App(int argc, const char **argv, const char *pager, uint flags)
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

    int argc;
    const char **argv;
    VPE vpe;
    RecvGate rgate;
    SendGate sgate;
};

int main(int argc, char **argv) {
    if(argc != 7) {
        cerr << "Usage: " << argv[0] << " <wrname> <rdname> <repeats> <data> <muxed> <instances>\n";
        return 1;
    }

    if(VERBOSE) cout << "Mounting filesystem...\n";

    if(VFS::mount("/", "m3fs") != Errors::NONE)
        PANIC("Cannot mount root fs");

    const char *wr_name = argv[1];
    const char *rd_name = argv[2];
    int repeats = IStringStream::read_from<int>(argv[3]);
    bool data = strcmp(argv[4], "1") == 0;
    bool muxed = strcmp(argv[5], "1") == 0;
    size_t instances = IStringStream::read_from<size_t>(argv[6]);

    App *apps[instances * 2];
    RemoteServer *srvs[3];
    VPE *srv_vpes[3];

    if(VERBOSE) cout << "Creating pager...\n";

    {
        srv_vpes[2] = new VPE("pager", VPE::self().pe(), "pager", muxed ? VPE::MUXABLE : 0);
        srvs[2] = new RemoteServer(*srv_vpes[2], "mypager");

        String srv_arg = srvs[2]->sel_arg();
        const char *args[] = {"/bin/pager", "-a", "16", "-f", "16", "-s", srv_arg.c_str()};
        Errors::Code res = srv_vpes[2]->exec(ARRAY_SIZE(args), args);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << args[0] << ": " << Errors::to_string(res));
    }

    if(VERBOSE) cout << "Creating application VPEs...\n";

    for(int j = 0; j < repeats; ++j) {
        const size_t ARG_COUNT = 11;
        for(size_t i = 0; i < instances * 2; ++i) {
            const char **args = new const char *[ARG_COUNT];
            args[0] = "/bin/fstrace-m3fs";

            apps[i] = new App(ARG_COUNT, args, "mypager", muxed ? VPE::MUXABLE | VPE::PINNED : 0);
        }

        if(j == 0 && VERBOSE) cout << "Creating servers...\n";

        if(j == 0) {
            srv_vpes[0] = new VPE("m3fs", VPE::self().pe(), "pager", muxed ? VPE::MUXABLE : 0);
            srvs[0] = new RemoteServer(*srv_vpes[0], "mym3fs");

            String srv_arg = srvs[0]->sel_arg();
            const char *args[] = {"/bin/m3fs", "-s", srv_arg.c_str(), "268435456"};
            Errors::Code res = srv_vpes[0]->exec(ARRAY_SIZE(args), args);
            if(res != Errors::NONE)
                PANIC("Cannot execute " << args[0] << ": " << Errors::to_string(res));
        }

        if(j == 0) {
            srv_vpes[1] = new VPE("pipeserv", VPE::self().pe(), "pager", muxed ? VPE::MUXABLE : 0);
            srvs[1] = new RemoteServer(*srv_vpes[1], "mypipe");

            String srv_arg = srvs[1]->sel_arg();
            const char *args[] = {"/bin/pipeserv", "-s", srv_arg.c_str()};
            Errors::Code res = srv_vpes[1]->exec(ARRAY_SIZE(args), args);
            if(res != Errors::NONE)
                PANIC("Cannot execute " << args[0] << ": " << Errors::to_string(res));
        }

        if(VERBOSE) cout << "Starting VPEs...\n";

        cycles_t overall_start = Time::start(0x1235);

        constexpr size_t PIPE_SHM_SIZE   = 512 * 1024;
        MemGate *mems[instances];
        IndirectPipe *pipes[instances];

        for(size_t i = 0; i < instances * 2; ++i) {
            OStringStream tmpdir(new char[16], 16);
            tmpdir << "/tmp/" << i << "/";
            const char **args = apps[i]->argv;
            args[1] = "-p";
            args[2] = tmpdir.str();
            args[3] = "-w";
            args[4] = "-i";
            args[5] = data ? "-d" : "-i";
            args[6] = "-f";
            args[7] = "mym3fs";
            args[8] = "-g";

            OStringStream rgatesel(new char[11], 11);
            rgatesel << apps[i]->rgate.sel() << " " << apps[i]->rgate.ep();
            args[9] = rgatesel.str();
            args[10] = (i % 2 == 0) ? wr_name : rd_name;

            if(VERBOSE) {
                cout << "Starting ";
                for(size_t x = 0; x < ARG_COUNT; ++x)
                    cout << args[x] << " ";
                cout << "\n";
            }

            if(i % 2 == 0) {
                mems[i / 2] = new MemGate(MemGate::create_global(PIPE_SHM_SIZE, MemGate::RW));
                pipes[i / 2] = new IndirectPipe(*mems[i / 2], PIPE_SHM_SIZE, "mypipe", data ? 0 : FILE_NODATA);
                apps[i]->vpe.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipes[i / 2]->writer_fd()));
            }
            else
                apps[i]->vpe.fds()->set(STDIN_FD, VPE::self().fds()->get(pipes[i / 2]->reader_fd()));
            apps[i]->vpe.obtain_fds();

            Errors::Code res = apps[i]->vpe.exec(apps[i]->argc, apps[i]->argv);
            if(res != Errors::NONE)
                PANIC("Cannot execute " << apps[i]->argv[0] << ": " << Errors::to_string(res));

            if(i % 2 == 1) {
                pipes[i / 2]->close_writer();
                pipes[i / 2]->close_reader();
            }
        }

        if(VERBOSE) cout << "Signaling VPEs...\n";

        for(size_t i = 0; i < instances * 2; ++i)
            send_receive_vmsg(apps[i]->sgate, 1);

        cycles_t start = Time::start(0x1234);

        for(size_t i = 0; i < instances * 2; ++i)
            send_vmsg(apps[i]->sgate, 1);

        if(VERBOSE) cout << "Waiting for VPEs...\n";

        for(size_t i = 0; i < instances * 2; ++i) {
            int res = apps[i]->vpe.wait();
            if(VERBOSE) cout << apps[i]->argv[0] << " exited with " << res << "\n";
        }

        cycles_t overall_end = Time::stop(0x1235);
        cycles_t end = Time::stop(0x1234);
        cout << "Time: " << (end - start) << ", total: " << (overall_end - overall_start) << "\n";

        if(VERBOSE) cout << "Deleting VPEs...\n";

        for(size_t i = 0; i < instances * 2; ++i) {
            delete pipes[i / 2];
            pipes[i / 2] = nullptr;
            delete mems[i / 2];
            mems[i / 2] = nullptr;
            delete apps[i];
        }
    }

    if(VERBOSE) cout << "Shutting down servers...\n";

    for(size_t i = 0; i < ARRAY_SIZE(srvs); ++i)
        srvs[i]->request_shutdown();

    for(size_t i = 0; i < ARRAY_SIZE(srvs); ++i) {
        int res = srv_vpes[i]->wait();
        if(VERBOSE) cout << "server " << i << " exited with " << res << "\n";
        delete srv_vpes[i];
        delete srvs[i];
    }

    if(VERBOSE) cout << "Done\n";
    return 0;
}
