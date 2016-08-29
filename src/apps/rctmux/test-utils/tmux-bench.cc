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

static const int TEST_COUNT = 50;

int main() {
    cout << "Time-multiplexing context-switch benchmark started.\n";

    cout << "Mounting filesystem...\n";
    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Cannot mount root fs");

    cout << "Starting two VPEs with time-multiplexing enabled\n";

    const char *args1[] = {"/bin/rctmux-util-counter"};
    const char *args2[] = {"/bin/unittests-misc"};

    // start the first vpe
    VPE s1(args1[0], VPE::self().pe(), "pager", true);
    s1.mountspace(*VPE::self().mountspace());
    s1.obtain_mountspace();
    Executable exec1(1, args1);
    Errors::Code res1 = s1.exec(exec1);
    if(res1 != Errors::NO_ERROR)
        PANIC("Cannot execute " << args1[0] << ": " << Errors::to_string(res1));

    // start the second VPE
    VPE s2(args2[0], VPE::self().pe(), "pager", true);
    s2.mountspace(*VPE::self().mountspace());
    s2.obtain_mountspace();
    Executable exec2(1, args2);
    Errors::Code res2 = s2.exec(exec2);
    if(res2 != Errors::NO_ERROR)
        PANIC("Cannot execute " << args2[0] << ": " << Errors::to_string(res2));

    // now do some switches
    cout << "Starting benchmark (" << TEST_COUNT << " switches)...\n";

    // switch between them until both exited
    size_t remaining = 2;
    capsel_t vpes[] = {s1.sel(), s2.sel()};
    for (int i = 0; remaining > 0 && i < TEST_COUNT; ++i) {
        if(vpes[i % 2] == ObjCap::INVALID)
            continue;

        // wait a bit
        for(volatile int i = 0; i < 10000; ++i)
            ;

        if(Syscalls::get().resume(vpes[i % 2]) != Errors::NO_ERROR) {
            // do not try that again for this VPE
            remaining--;
            vpes[i % 2] = ObjCap::INVALID;
            cout << "resume failed\n";
        }
    }

    cout << "Benchmark finished.\n";

    return 0;
}
