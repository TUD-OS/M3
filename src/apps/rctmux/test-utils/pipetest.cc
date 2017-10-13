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
#include <base/util/Profile.h>
#include <base/Panic.h>

#include <m3/stream/Standard.h>
#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

#define VERBOSE     0

static const size_t PIPE_SHM_SIZE   = 128 * 1024;
static const int REPEATS            = 8;

struct App {
    explicit App(int argc, const char *argv[], bool muxed)
        : argc(argc), argv(argv),
          vpe(argv[0], VPE::self().pe(), "pager", muxed) {
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to create VPE");
    }

    int argc;
    const char **argv;
    VPE vpe;
};

static App *create(int no, int argc, char **argv, bool muxable) {
    if(VERBOSE) cout << "VPE" << no << ": ";
    const char **args2 = new const char*[argc];
    for(int i = 0; i < argc; ++i) {
        args2[i] = argv[i];
        if(VERBOSE) cout << args2[i] << " ";
    }
    if(VERBOSE) cout << "\n";
    return new App(argc, args2, muxable);
}

static void wait_for(const char *service) {
    while(1) {
        for(volatile int x = 0; x < 10000; ++x)
            ;

        Session *sess = new Session(service);
        if(sess->is_connected()) {
            delete sess;
            break;
        }
    }
}

static void usage(const char *name) {
    cerr << "Usage: " << name << " <mode> <mem> <wargs> <rargs> ...\n";
    cerr << " <mode> can be:\n";
    cerr << " 0: not muxable\n";
    cerr << " 1: all muxable\n";
    cerr << " 2: m3fs with pipe\n";
    cerr << " 3: mux m3fs with pipe\n";
    cerr << " <mem> can be:\n";
    cerr << " 0: DRAM\n";
    cerr << " 1: SPM\n";
    exit(1);
}

int main(int argc, char **argv) {
    if(argc < 5)
        usage(argv[0]);

    if(VERBOSE) cout << "Mounting filesystem...\n";

    if(VFS::mount("/", "m3fs") != Errors::NONE)
        PANIC("Cannot mount root fs");

    int mode = IStringStream::read_from<int>(argv[1]);
    int mem = IStringStream::read_from<int>(argv[2]);
    int wargs = IStringStream::read_from<int>(argv[3]);
    int rargs = IStringStream::read_from<int>(argv[4]);

    if(argc != 5 + wargs + rargs)
        usage(argv[0]);

    MemGate pipemem = MemGate::create_global(PIPE_SHM_SIZE, MemGate::RW);

    for(int j = 0; j < REPEATS; ++j) {
        App *apps[4];

        if(VERBOSE) cout << "Creating VPEs...\n";

        const char *pipeserv[] = {"/bin/pipeserv"};
        const char *m3fs[] = {"/bin/m3fs", "-n=m3fs2", "67108864"};
        if(mode < 2) {
            apps[0] = create(0, ARRAY_SIZE(pipeserv), const_cast<char**>(pipeserv), mode == 1);
            apps[1] = nullptr;
            apps[2] = create(1, wargs, argv + 5, mode == 1);
            apps[3] = create(2, rargs, argv + 5 + wargs, mode == 1);
        }
        else {
            apps[2] = create(1, wargs, argv + 5, false);
            apps[3] = create(2, rargs, argv + 5 + wargs, false);
            apps[0] = create(0, ARRAY_SIZE(pipeserv), const_cast<char**>(pipeserv), mode == 3);
            apps[1] = create(3, ARRAY_SIZE(m3fs), const_cast<char**>(m3fs), mode == 3);
        }

        if(VERBOSE) cout << "Starting service...\n";

        // start service
        Errors::Code res = apps[0]->vpe.exec(apps[0]->argc, apps[0]->argv);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << apps[0]->argv[0] << ": " << Errors::to_string(res));

        if(apps[1]) {
            res = apps[1]->vpe.exec(apps[1]->argc, apps[1]->argv);
            if(res != Errors::NONE)
                PANIC("Cannot execute " << apps[1]->argv[0] << ": " << Errors::to_string(res));
        }

        if(VERBOSE) cout << "Waiting for service...\n";

        // the kernel does not block us atm until the service is available
        // so try to connect until it's available
        wait_for("pipe");
        if(apps[1])
            wait_for("m3fs2");

        MemGate *vpemem = nullptr;
        VPE *memvpe = nullptr;
        IndirectPipe *pipe;
        if(mem == 0)
            pipe = new IndirectPipe(pipemem, PIPE_SHM_SIZE);
        else {
            memvpe = new VPE("mem");
            vpemem = new MemGate(memvpe->mem().derive(0x10000, PIPE_SHM_SIZE, MemGate::RW));
            pipe = new IndirectPipe(*vpemem, PIPE_SHM_SIZE);
        }

        if(VERBOSE) cout << "Starting reader and writer...\n";

        if(apps[1]) {
            if(VFS::mount("/foo", "m3fs", "m3fs2") != Errors::NONE)
                PANIC("Cannot mount root fs");
        }

        cycles_t start = Profile::start(0x1234);

        // start writer
        apps[2]->vpe.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe->writer_fd()));
        apps[2]->vpe.obtain_fds();
        apps[2]->vpe.mounts(*VPE::self().mounts());
        apps[2]->vpe.obtain_mounts();
        res = apps[2]->vpe.exec(apps[2]->argc, apps[2]->argv);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << apps[2]->argv[0] << ": " << Errors::to_string(res));

        // start reader
        apps[3]->vpe.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe->reader_fd()));
        apps[3]->vpe.obtain_fds();
        apps[3]->vpe.mounts(*VPE::self().mounts());
        apps[3]->vpe.obtain_mounts();
        res = apps[3]->vpe.exec(apps[3]->argc, apps[3]->argv);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << apps[3]->argv[0] << ": " << Errors::to_string(res));

        pipe->close_writer();
        pipe->close_reader();

        if(VERBOSE) cout << "Waiting for VPEs...\n";

        // don't wait for the services
        for(size_t i = 2; i < ARRAY_SIZE(apps); ++i) {
            int res = apps[i]->vpe.wait();
            if(VERBOSE) cout << apps[i]->argv[0] << " exited with " << res << "\n";
        }

        cycles_t end = Profile::stop(0x1234);
        cout << "Time: " << (end - start) << "\n";

        if(VERBOSE) cout << "Deleting VPEs...\n";

        if(apps[1])
            VFS::unmount("/foo");

        for(size_t i = 0; i < ARRAY_SIZE(apps); ++i)
            delete apps[i];
        delete pipe;
        delete vpemem;
        delete memvpe;

        if(VERBOSE) cout << "Done\n";
    }
    return 0;
}
