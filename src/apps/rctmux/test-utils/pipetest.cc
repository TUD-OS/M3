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
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

#define VERBOSE     0

static const size_t PIPE_SHM_SIZE   = 512 * 1024;
static const int REPEATS            = 8;

struct App {
    explicit App(const char *name, bool muxed)
        : name(name),
          vpe(name, VPE::self().pe(), "pager", muxed) {
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to create VPE");
    }

    const char *name;
    VPE vpe;
};

struct RemoteServer {
    explicit RemoteServer(VPE &vpe, const String &name)
        : srv(ObjCap::SERVICE, VPE::self().alloc_sels(2)),
          rgate(RecvGate::create_for(vpe, srv.sel() + 1, nextlog2<256>::val, nextlog2<256>::val)) {
        rgate.activate();
        Syscalls::get().createsrv(srv.sel(), vpe.sel(), rgate.sel(), name);
        vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, srv.sel(), 2));
    }
    ~RemoteServer() {
        Syscalls::get().srvctrl(srv.sel(), KIF::Syscall::SCTRL_SHUTDOWN);
    }

    const char *sel_arg() const {
        static char buffer[32];
        OStringStream os(buffer, sizeof(buffer));
        os << srv.sel() << " " << rgate.ep();
        return os.str();
    }

    ObjCap srv;
    RecvGate rgate;
};

static App *create(int no, const char *name, bool muxable) {
    if(VERBOSE) cout << "VPE" << no << ": " << name << "\n";
    return new App(name, muxable);
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

int main(int argc, const char **argv) {
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

        const char **wargv = argv + 5;
        const char **rargv = argv + 5 + wargs;
        if(mode < 2) {
            apps[0] = create(0, "pipeserv", mode == 1);
            apps[1] = nullptr;
            apps[2] = create(1, wargv[0], mode == 1);
            apps[3] = create(2, rargv[0], mode == 1);
        }
        else {
            apps[2] = create(1, wargv[0], false);
            apps[3] = create(2, rargv[0], false);
            apps[0] = create(0, "pipeserv", mode == 3);
            apps[1] = create(3, "m3fs", mode == 3);
        }

        RemoteServer *m3fs_srv = nullptr;
        RemoteServer *pipe_srv = new RemoteServer(apps[0]->vpe, "pipe");
        if(apps[1])
            m3fs_srv = new RemoteServer(apps[1]->vpe, "mym3fs");

        if(VERBOSE) cout << "Starting services...\n";

        // start services
        const char *pipe_args[] = {"/bin/pipeserv", "-s", pipe_srv->sel_arg()};
        Errors::Code res = apps[0]->vpe.exec(ARRAY_SIZE(pipe_args), pipe_args);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << pipe_args[0] << ": " << Errors::to_string(res));

        if(apps[1]) {
            const char *m3fs_args[] = {"/bin/m3fs", "-s", m3fs_srv->sel_arg(), "67108864"};
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
        apps[2]->vpe.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe->writer_fd()));
        apps[2]->vpe.obtain_fds();
        apps[2]->vpe.mounts(*VPE::self().mounts());
        apps[2]->vpe.obtain_mounts();
        res = apps[2]->vpe.exec(wargs, wargv);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << wargv[0] << ": " << Errors::to_string(res));

        // start reader
        apps[3]->vpe.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe->reader_fd()));
        apps[3]->vpe.obtain_fds();
        apps[3]->vpe.mounts(*VPE::self().mounts());
        apps[3]->vpe.obtain_mounts();
        res = apps[3]->vpe.exec(rargs, rargv);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << rargv[0] << ": " << Errors::to_string(res));

        pipe->close_writer();
        pipe->close_reader();

        if(VERBOSE) cout << "Waiting for applications...\n";

        // don't wait for the services
        for(size_t i = 2; i < ARRAY_SIZE(apps); ++i) {
            int res = apps[i]->vpe.wait();
            if(VERBOSE) cout << apps[i]->name << " exited with " << res << "\n";
        }

        cycles_t end = Time::stop(0x1234);
        cout << "Time: " << (end - start) << "\n";

        if(VERBOSE) cout << "Waiting for services...\n";

        // destroy pipe first
        delete pipe;
        delete vpemem;
        delete memvpe;

        // request shutdown
        delete m3fs_srv;
        delete pipe_srv;

        // wait for services
        for(size_t i = 0; i < 2; ++i) {
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

        if(VERBOSE) cout << "Done\n";
    }
    return 0;
}
