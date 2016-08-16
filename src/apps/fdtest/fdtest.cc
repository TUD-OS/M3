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

#include <base/Common.h>

#include <m3/session/M3FS.h>
#include <m3/stream/FStream.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/MountSpace.h>
#include <m3/vfs/SerialFile.h>
#include <m3/vfs/VFS.h>

using namespace m3;

static char buffer[1024];

int main() {
    if(VFS::mount("/", new M3FS("m3fs")) < 0) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Mounting root-fs failed");
    }

    cout << "Hello World!\n";
    cout.flush();

    cout << "Enter your name: ";
    cout.flush();

    String name;
    cin >> name;

    cout << "Your name is: " << name << "\n";
    cout.flush();

    {
        FStream f("/test.txt", FILE_R);

        VPE child("child");

        child.mountspace(*VPE::self().mountspace());
        child.obtain_mountspace();

        child.fds(*VPE::self().fds());
        child.obtain_fds();

        child.run([&f] {
            f.read(buffer, sizeof(buffer));
            cout << buffer << "\n";
            return 0;
        });
        int res = child.wait();
        cout << "result: " << res << "\n";
    }

    {
        FStream f("/pat.bin", FILE_R, 128);

        size_t size = f.read(buffer, 128);
        cout.dump(buffer, size);

        VPE child("child");

        child.mountspace(*VPE::self().mountspace());
        child.obtain_mountspace();

        child.fds()->set(STDIN_FD, f.file());
        child.obtain_fds();

        const char *args[] = {"/bin/fdchild"};
        child.exec(ARRAY_SIZE(args), args);
        int res = child.wait();
        cout << "result: " << res << "\n";
    }
    return 0;
}
