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
#include <base/util/Sync.h>
#include <base/util/Profile.h>
#include <base/Panic.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/Executable.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

#define VERBOSE     0
#define REPEATS     4

struct App {
    explicit App(int argc, const char *argv[], bool muxed)
        : exec(argc, argv),
          vpe(argv[0], VPE::self().pe(), "pager", muxed) {
    }

    Executable exec;
    VPE vpe;
};

int main(int argc, char **argv) {
    if(argc < 4) {
        cerr << "Usage: " << argv[0] << " 1|0 <argcount> <prog1>...\n";
        return 1;
    }

    if(VERBOSE) cout << "Mounting filesystem...\n";

    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Cannot mount root fs");

    bool muxed = strcmp(argv[1], "1") == 0;
    int argcount = IStringStream::read_from<int>(argv[2]);
    App *apps[(argc - 3) / argcount];

    for(int j = 0; j < REPEATS; ++j) {
        if(VERBOSE) cout << "Creating VPEs...\n";

        size_t idx = 0;
        for(int i = 3; i < argc; i += argcount) {
            const char **args = new const char*[argcount];
            for(int x = 0; x < argcount; ++x)
                args[x] = argv[i + x];
            if(VERBOSE) {
                cout << "Creating ";
                for(int x = 0; x < argcount; ++x)
                    cout << args[x] << " ";
                cout << "\n";
            }
            apps[idx++] = new App(argcount, args, muxed);
        }

        if(VERBOSE) cout << "Starting VPEs...\n";

        cycles_t start = Profile::start(0x1234);

        for(size_t i = 0; i < ARRAY_SIZE(apps); ++i) {
            apps[i]->vpe.mountspace(*VPE::self().mountspace());
            apps[i]->vpe.obtain_mountspace();
            Errors::Code res = apps[i]->vpe.exec(apps[i]->exec);
            if(res != Errors::NO_ERROR)
                PANIC("Cannot execute " << apps[i]->exec.argv()[0] << ": " << Errors::to_string(res));

            if(!muxed) {
                if(VERBOSE) cout << "Waiting for VPE " << apps[i]->exec.argv()[0] << " ...\n";

                UNUSED int res = apps[i]->vpe.wait();
                if(VERBOSE) cout << apps[i]->exec.argv()[0] << " exited with " << res << "\n";
            }
        }

        if(muxed) {
            if(VERBOSE) cout << "Waiting for VPEs...\n";

            for(size_t i = 0; i < ARRAY_SIZE(apps); ++i) {
                int res = apps[i]->vpe.wait();
                if(VERBOSE) cout << apps[i]->exec.argv()[0] << " exited with " << res << "\n";
            }
        }

        cycles_t end = Profile::stop(0x1234);
        cout << "Time: " << (end - start) << "\n";

        if(VERBOSE) cout << "Deleting VPEs...\n";

        for(size_t i = 0; i < ARRAY_SIZE(apps); ++i)
            delete apps[i];
    }

    if(VERBOSE) cout << "Done\n";
    return 0;
}
