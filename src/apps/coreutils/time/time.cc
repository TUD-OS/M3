/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/util/Time.h>

#include <m3/stream/Standard.h>
#include <m3/VPE.h>

using namespace m3;

int main(int argc, char **argv) {
    if(argc < 2)
        exitmsg("Usage: " << argv[0] << " <file>...");

    int res;
    cycles_t start = Time::start(0);
    {
        VPE child(argv[1]);
        if(Errors::last != Errors::NONE)
            exitmsg("Creating VPE for " << argv[1] << " failed");

        child.fds()->set(STDIN_FD, VPE::self().fds()->get(STDIN_FD));
        child.fds()->set(STDOUT_FD, VPE::self().fds()->get(STDOUT_FD));
        child.fds()->set(STDERR_FD, VPE::self().fds()->get(STDERR_FD));
        child.obtain_fds();

        child.mounts(*VPE::self().mounts());
        child.obtain_mounts();

        Errors::Code err = child.exec(argc - 1, const_cast<const char**>(argv) + 1);
        if(err != Errors::NONE)
            exitmsg("Executing " << argv[1] << " failed");

        res = child.wait();
    }

    cycles_t end = Time::stop(0);

    cerr << "VPE (" << argv[1] << ") terminated with exit-code " << res << "\n";
    cerr << "Runtime: " << (end - start) << " cycles\n";
    return 0;
}
