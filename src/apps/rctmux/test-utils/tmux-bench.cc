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

#include <m3/Common.h>
#include <m3/stream/Serial.h>
#include <m3/Syscalls.h>
#include <m3/vfs/VFS.h>
#include <m3/cap/VPE.h>
#include <m3/Log.h>

#include <m3/util/Sync.h>
#include <m3/util/Profile.h>

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

    Serial::get() << "Time-multiplexing context-switch benchmark started.\n";

    Serial::get() << "Mounting filesystem...\n";
    if(VFS::mount("/", new M3FS("m3fs")) < 0)
        PANIC("Cannot mount root fs");

    Serial::get() << "Starting two VPEs with time-multiplexing enabled\n";

    const char *args1[] = { "/bin/rctmux-util-counter"};
    const char *args2[] = { "/bin/unittests-misc"};

    // start the first vpe
    VPE s1(args1[0], "", "pager", true);
    s1.delegate_mounts();
    START(s1, args1);

    // start the second VPE
    VPE s2(args2[0], "", "pager", true);
    s2.delegate_mounts();
    START(s2, args2);

    // now do some switches
#define COUNT 50
    Serial::get() << "Starting benchmark (" << COUNT << " switches)...\n";
    _switchalot(COUNT);
    Serial::get() << "Benchmark finished.\n";

    return 0;
}
