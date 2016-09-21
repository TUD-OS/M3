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
#include <base/util/Sync.h>
#include <base/util/Profile.h>
#include <base/Panic.h>

#include <m3/stream/Standard.h>
#include <m3/pipe/IndirectPipe.h>
#include <m3/vfs/Executable.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

#define VERBOSE     0

struct App {
    explicit App(int argc, const char *argv[], bool muxed)
        : exec(argc, argv),
          vpe(argv[0], VPE::self().pe(), "pager", muxed) {
    }

    Executable exec;
    VPE vpe;
};

int main(int argc, char **argv) {
    if(argc < 3) {
        cerr << "Usage: " << argv[0] << " 1|0 <rargs> ...\n";
        return 1;
    }

    if(VERBOSE) cout << "Mounting filesystem...\n";

    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Cannot mount root fs");

    if(VERBOSE) cout << "Creating VPEs...\n";

    bool muxed = strcmp(argv[1], "1") == 0;
    size_t rargs = IStringStream::read_from<size_t>(argv[2]);

    App *apps[3];

    const char *args1[] = {"/bin/pipeserv"};
    apps[0] = new App(ARRAY_SIZE(args1), args1, muxed);
    if(VERBOSE) cout << "VPE1: " << args1[0] << "\n";

    if(VERBOSE) cout << "VPE2: ";
    const char **args2 = new const char*[rargs];
    for(size_t i = 0; i < rargs; ++i) {
        args2[i] = argv[3 + i];
        if(VERBOSE) cout << args2[i] << " ";
    }
    if(VERBOSE) cout << "\n";
    apps[1] = new App(rargs, args2, muxed);

    if(VERBOSE) cout << "VPE3: ";
    const char **args3 = new const char*[argc - (3 + rargs)];
    for(int i = 3 + rargs; i < argc; ++i) {
        args3[i - (3 + rargs)] = argv[i];
        if(VERBOSE) cout << argv[i] << " ";
    }
    if(VERBOSE) cout << "\n";
    apps[2] = new App(argc - (3 + rargs), args3, muxed);

    if(VERBOSE) cout << "Starting service...\n";

    // start service
    Errors::Code res = apps[0]->vpe.exec(apps[0]->exec);
    if(res != Errors::NO_ERROR)
        PANIC("Cannot execute " << apps[0]->exec.argv()[0] << ": " << Errors::to_string(res));

    if(VERBOSE) cout << "Waiting for service...\n";

    // the kernel does not block us atm until the service is available
    // so try to connect until it's available
    while(1) {
        Session *sess = new Session("pipe");
        if(sess->is_connected()) {
            delete sess;
            break;
        }

        for(volatile int x = 0; x < 10000; ++x)
            ;
    }

    IndirectPipe pipe(64 * 1024);

    if(VERBOSE) cout << "Starting reader and writer...\n";

    cycles_t start = Profile::start(0x1234);

    // start writer
    apps[1]->vpe.fds()->set(STDOUT_FD, VPE::self().fds()->get(pipe.writer_fd()));
    apps[1]->vpe.obtain_fds();
    apps[1]->vpe.mountspace(*VPE::self().mountspace());
    apps[1]->vpe.obtain_mountspace();
    res = apps[1]->vpe.exec(apps[1]->exec);
    if(res != Errors::NO_ERROR)
        PANIC("Cannot execute " << apps[1]->exec.argv()[0] << ": " << Errors::to_string(res));

    // start reader
    apps[2]->vpe.fds()->set(STDIN_FD, VPE::self().fds()->get(pipe.reader_fd()));
    apps[2]->vpe.obtain_fds();
    res = apps[2]->vpe.exec(apps[2]->exec);
    if(res != Errors::NO_ERROR)
        PANIC("Cannot execute " << apps[2]->exec.argv()[0] << ": " << Errors::to_string(res));

    pipe.close_writer();
    pipe.close_reader();

    if(VERBOSE) cout << "Waiting for VPEs...\n";

    // don't wait for the service
    for(size_t i = 1; i < ARRAY_SIZE(apps); ++i) {
        int res = apps[i]->vpe.wait();
        if(VERBOSE) cout << apps[i]->exec.argv()[0] << " exited with " << res << "\n";
    }

    cycles_t end = Profile::stop(0x1234);
    cout << "Time: " << (end - start) << "\n";

    if(VERBOSE) cout << "Deleting VPEs...\n";

    for(size_t i = 0; i < ARRAY_SIZE(apps); ++i)
        delete apps[i];

    if(VERBOSE) cout << "Done\n";
    return 0;
}
