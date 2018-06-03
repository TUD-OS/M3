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

#include <m3/stream/Standard.h>
#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>

#include "Helper.h"

using namespace m3;

#define VERBOSE     0

static const size_t PIPE_SHM_SIZE   = 512 * 1024;

struct App {
    explicit App(const char *name, const char *pager, bool muxed)
        : name(name),
          vpe(name, VPE::self().pe(), pager, muxed) {
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to create VPE");
    }

    const char *name;
    VPE vpe;
};

static App *create(const char *name, const char *pager, bool muxable) {
    if(VERBOSE) cout << "VPE: " << name << "\n";
    return new App(name, pager, muxable);
}

static void usage(const char *name) {
    cerr << "Usage: " << name << " <mode> <mem> <repeats> <wargs> <rargs> ...\n";
    cerr << " <mode> can be:\n";
    cerr << " 0: not muxable\n";
    cerr << " 1: all muxable\n";
    cerr << " 2: m3fs with pipe\n";
    cerr << " 3: mux m3fs with pipe\n";
    cerr << " 4: mux m3fs with pipe and apps\n";
    cerr << " <mem> can be:\n";
    cerr << " 0: DRAM\n";
    cerr << " 1: SPM\n";
    exit(1);
}

int main(int argc, const char **argv) {
    if(argc < 6)
        usage(argv[0]);

    if(VERBOSE) cout << "Mounting filesystem...\n";

    if(VFS::mount("/", "m3fs") != Errors::NONE)
        PANIC("Cannot mount root fs");

    int mode = IStringStream::read_from<int>(argv[1]);
    int mem = IStringStream::read_from<int>(argv[2]);
    int repeats = IStringStream::read_from<int>(argv[3]);
    int wargs = IStringStream::read_from<int>(argv[4]);
    int rargs = IStringStream::read_from<int>(argv[5]);

    if(argc != 6 + wargs + rargs)
        usage(argv[0]);

    MemGate pipemem = MemGate::create_global(PIPE_SHM_SIZE, MemGate::RW);

    for(int j = 0; j < repeats; ++j) {
        App *apps[5];

        if(VERBOSE) cout << "Creating VPEs...\n";

        // start pager
        apps[2] = create("pager", "pager", mode >= 3);
        RemoteServer *pagr_srv = new RemoteServer(apps[2]->vpe, "mypager");

        {
            String pgarg = pagr_srv->sel_arg();
            const char *pager_args[] = {"/bin/pager", "-a", "16", "-f", "16", "-s", pgarg.c_str()};
            Errors::Code res = apps[2]->vpe.exec(ARRAY_SIZE(pager_args), pager_args);
            if(res != Errors::NONE)
                PANIC("Cannot execute " << pager_args[0] << ": " << Errors::to_string(res));
        }

        const char **wargv = argv + 6;
        const char **rargv = argv + 6 + wargs;
        if(mode < 2) {
            apps[0] = create("pipeserv", "pager", mode == 1);
            apps[1] = nullptr;
            apps[3] = create(wargv[0], "mypager", mode == 1);
            apps[4] = create(rargv[0], "mypager", mode == 1);
        }
        else {
            apps[3] = create(wargv[0], "mypager", mode == 4);
            apps[4] = create(rargv[0], "mypager", mode == 4);
            apps[0] = create("pipeserv", "pager", mode >= 3);
            apps[1] = create("m3fs", "pager", mode >= 3);
        }

        RemoteServer *m3fs_srv = nullptr;
        RemoteServer *pipe_srv = new RemoteServer(apps[0]->vpe, "pipe");
        if(apps[1])
            m3fs_srv = new RemoteServer(apps[1]->vpe, "mym3fs");

        if(VERBOSE) cout << "Starting services...\n";

        // start services
        String pipearg = pipe_srv->sel_arg();
        const char *pipe_args[] = {"/bin/pipeserv", "-s", pipearg.c_str()};
        Errors::Code res = apps[0]->vpe.exec(ARRAY_SIZE(pipe_args), pipe_args);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << pipe_args[0] << ": " << Errors::to_string(res));

        if(apps[1]) {
            String m3fsarg = m3fs_srv->sel_arg();
            const char *m3fs_args[] = {"/bin/m3fs", "-s", m3fsarg.c_str(), "134217728"};
            res = apps[1]->vpe.exec(ARRAY_SIZE(m3fs_args), m3fs_args);
            if(res != Errors::NONE)
                PANIC("Cannot execute " << m3fs_args[0] << ": " << Errors::to_string(res));
        }

        // create pipe
        MemGate *vpemem = nullptr;
        VPE *memvpe = nullptr;
        IndirectPipe *pipe;
        if(mem == 0)
            pipe = new IndirectPipe(pipemem, PIPE_SHM_SIZE);
        else {
            memvpe = new VPE("mem");
            vpemem = new MemGate(memvpe->mem().derive(0x10000, PIPE_SHM_SIZE, MemGate::RW));
            pipe = new IndirectPipe(*vpemem, PIPE_SHM_SIZE);
            // let the kernel schedule the VPE; this cannot be done by the reader/writer, because
            // the pipe service just configures their EP, but doesn't delegate the memory capability
            // to them
            vpemem->write(&vpemem, sizeof(vpemem), 0);
        }

        if(VERBOSE) cout << "Starting reader and writer...\n";

        if(apps[1]) {
            if(VFS::mount("/foo", "m3fs", "mym3fs") != Errors::NONE)
                PANIC("Cannot mount root fs");
        }

        cycles_t start = Time::start(0x1234);

        // start writer
        apps[3]->vpe.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe->writer_fd()));
        apps[3]->vpe.obtain_fds();
        apps[3]->vpe.mounts(*VPE::self().mounts());
        apps[3]->vpe.obtain_mounts();
        res = apps[3]->vpe.exec(wargs, wargv);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << wargv[0] << ": " << Errors::to_string(res));

        // start reader
        apps[4]->vpe.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe->reader_fd()));
        apps[4]->vpe.obtain_fds();
        apps[4]->vpe.mounts(*VPE::self().mounts());
        apps[4]->vpe.obtain_mounts();
        res = apps[4]->vpe.exec(rargs, rargv);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << rargv[0] << ": " << Errors::to_string(res));

        pipe->close_writer();
        pipe->close_reader();

        if(VERBOSE) cout << "Waiting for applications...\n";
        cycles_t runstart = Time::start(0x1111);

        // don't wait for the services
        for(size_t i = 3; i < ARRAY_SIZE(apps); ++i) {
            int res = apps[i]->vpe.wait();
            if(VERBOSE) cout << apps[i]->name << " exited with " << res << "\n";
        }

        cycles_t runend = Time::stop(0x1111);
        cycles_t end = Time::stop(0x1234);
        cout << "Time: " << (end - start) << ", runtime: " << (runend - runstart) << "\n";

        if(VERBOSE) cout << "Waiting for services...\n";

        // destroy pipe first
        delete pipe;
        delete vpemem;
        delete memvpe;

        // request shutdown
        pipe_srv->request_shutdown();
        pagr_srv->request_shutdown();
        if(m3fs_srv)
            m3fs_srv->request_shutdown();

        // wait for services
        for(size_t i = 0; i < 3; ++i) {
            if(!apps[i])
                continue;
            int res = apps[i]->vpe.wait();
            if(VERBOSE) cout << apps[i]->name << " exited with " << res << "\n";
        }

        if(VERBOSE) cout << "Deleting VPEs...\n";

        if(apps[1])
            VFS::unmount("/foo");

        for(size_t i = 0; i < ARRAY_SIZE(apps); ++i)
            delete apps[i];
        delete m3fs_srv;
        delete pipe_srv;
        delete pagr_srv;

        if(VERBOSE) cout << "Done\n";
    }
    return 0;
}
