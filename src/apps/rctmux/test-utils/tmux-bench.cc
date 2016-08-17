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
#include <m3/Syscalls.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>
#include <m3/stream/Standard.h>

#include <base/util/Sync.h>
#include <base/util/Profile.h>
#include <base/Panic.h>

using namespace m3;

template <typename T>
void unused(T &&) {}

#define START(x, y) { \
    Errors::Code c = x.exec(1, y); \
    if (c != Errors::NO_ERROR) \
        PANIC("Cannot execute " << y[0] << ": " << Errors::to_string(c)); }

/*void _pseudo_sleep(int ticks) {
    for (int i = 0; i < ticks * 1000; ++i)
        if (!(i % 1000))
            Serial::get() << "TICK\n";
}*/

void _switchalot(int count) {
    for (int i = 0; i < count; ++i) {
        Syscalls::get().tmuxswitch();
    }
}

int main(int argc, char **argv)
{
    unused(argc);
    unused(argv);

    cout << "Time-multiplexing context-switch benchmark started.\n";

    cout << "Mounting filesystem...\n";
    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Cannot mount root fs");

    cout << "Starting two VPEs with time-multiplexing enabled\n";

    const char *args1[] = { "/bin/rctmux-util-counter"};
    const char *args2[] = { "/bin/unittests-misc"};

    // start the first vpe
    VPE s1(args1[0], VPE::self().pe(), "pager", true);
    s1.mountspace(*VPE::self().mountspace());
    s1.obtain_mountspace();
    START(s1, args1);

    // start the second VPE
    VPE s2(args2[0], VPE::self().pe(), "pager", true);
    s2.mountspace(*VPE::self().mountspace());
    s2.obtain_mountspace();
    START(s2, args2);

    // now do some switches
#define COUNT 50
    cout << "Starting benchmark (" << COUNT << " switches)...\n";
    _switchalot(COUNT);
    cout << "Benchmark finished.\n";

    return 0;
}
