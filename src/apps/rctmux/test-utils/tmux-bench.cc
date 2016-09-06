/**
* Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
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
#include <base/util/Sync.h>
#include <base/util/Profile.h>
#include <base/Panic.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/Executable.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

struct App {
    explicit App(int argc, const char *argv[])
        : exec(argc, argv),
          vpe(argv[0], VPE::self().pe(), "pager", true) {
    }

    Executable exec;
    VPE vpe;
};

int main() {
    cout << "Mounting filesystem...\n";
    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Cannot mount root fs");

    cout << "Creating VPEs...\n";

    App *apps[2];

    const char *args1[] = {"/bin/rctmux-util-counter"};
    apps[0] = new App(ARRAY_SIZE(args1), args1);

    const char *args2[] = {"/bin/unittests-misc"};
    apps[1] = new App(ARRAY_SIZE(args2), args2);

    cout << "Starting VPEs...\n";

    for(size_t i = 0; i < ARRAY_SIZE(apps); ++i) {
        apps[i]->vpe.mountspace(*VPE::self().mountspace());
        apps[i]->vpe.obtain_mountspace();
        Errors::Code res = apps[i]->vpe.exec(apps[i]->exec);
        if(res != Errors::NO_ERROR)
            PANIC("Cannot execute " << apps[i]->exec.argv()[0] << ": " << Errors::to_string(res));
    }

    cout << "Waiting for VPEs...\n";

    for(size_t i = 0; i < ARRAY_SIZE(apps); ++i) {
        int res = apps[i]->vpe.wait();
        cout << apps[i]->exec.argv()[0] << " exited with " << res << "\n";
    }

    cout << "Deleting VPEs...\n";

    for(size_t i = 0; i < ARRAY_SIZE(apps); ++i)
        delete apps[i];

    cout << "Done\n";
    return 0;
}
