/*
 * Copyright (C) 2015-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
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

#include <m3/session/M3FS.h>
#include <m3/session/VTerm.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

int main(int argc, const char **argv) {
    if(argc < 2)
        exitmsg("Usage: " << argv[0] << " <program> [<arg>...]");

    if(VFS::mount("/", "m3fs") != Errors::NONE) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Mounting root-fs failed");
    }

    VPE sh(argv[1], VPE::self().pe(), "pager");
    if(Errors::last != Errors::NONE)
        exitmsg("Unable to create VPE");

    VTerm vterm("vterm");
    if(vterm.is_connected()) {
        sh.fds()->set(STDIN_FD, vterm.create_channel(true));
        sh.fds()->set(STDOUT_FD, vterm.create_channel(false));
        sh.fds()->set(STDERR_FD, vterm.create_channel(false));
        sh.obtain_fds();
    }

    sh.mounts(*VPE::self().mounts());
    sh.obtain_mounts();

    if(sh.exec(argc - 1, argv + 1) != Errors::NONE)
        exitmsg("Unable to exec " << argv[1]);

    sh.wait();
    return 0;
}
