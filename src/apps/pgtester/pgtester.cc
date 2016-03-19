/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Log.h>

#include <m3/session/M3FS.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/Executable.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

using namespace m3;

int main() {
    if(VFS::mount("/", new M3FS("m3fs")) < 0) {
        if(Errors::last != Errors::EXISTS)
            PANIC("Mounting root-fs failed");
    }

    {
        VPE child("child", "", "pager");
        child.delegate_mounts();

        const char *args[] = {"/bin/pgchild"};
        Executable exec(ARRAY_SIZE(args), args);

        child.exec(exec);
        child.wait();
    }

    {
        VPE child("child", "", "pager");
        child.delegate_mounts();

        const char *args[] = {"/bin/bench-syscall"};
        Executable exec(ARRAY_SIZE(args), args);

        child.exec(exec);
        child.wait();
    }

    Serial::get() << "Bye World!\n";
    return 0;
}
