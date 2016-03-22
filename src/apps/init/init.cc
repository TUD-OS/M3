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
#include <base/util/Sync.h>

#include <m3/com/MemGate.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/File.h>
#include <m3/vfs/Dir.h>
#include <m3/VPE.h>

using namespace m3;

static int myvar = 0;

static int myfunc(MemGate &mem) {
    cout << "myvar: " << myvar << "\n";

    alignas(DTU_PKG_SIZE) char buffer[16];
    mem.read_sync(buffer, sizeof(buffer), 0);
    for(size_t i = 0; i < sizeof(buffer); ++i)
        cout << i << ": " << fmt(buffer[i], "#02x") << "\n";

    const char *dirname = "/bin";
    Dir dir(dirname);
    if(Errors::occurred())
        exitmsg("open of " << dirname << " failed");

    cout << "Listing dir " << dirname << "...\n";
    Dir::Entry e;
    while(dir.readdir(e))
        cout << " Found " << e.name << " -> " << e.nodeno << "\n";
    return 0;
}

int main() {
    if(VFS::mount("/", new M3FS("m3fs")) < 0) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Mounting root-fs failed");
    }

    MemGate mem = MemGate::create_global(0x1000, MemGate::RW);
    alignas(DTU_PKG_SIZE) char buffer[16];
    for(size_t i = 0; i < sizeof(buffer); ++i)
        buffer[i] = i;
    mem.write_sync(buffer, sizeof(buffer), 0);

    myvar = 42;

    int code;

    {
        VPE vpe("hello");

        vpe.mountspace(*VPE::self().mountspace());
        vpe.obtain_mountspace();

        vpe.fds(*VPE::self().fds());
        vpe.obtain_fds();

        const char *args[] = {"/bin/hello", "foo", "bar"};
        Errors::Code res = vpe.exec(ARRAY_SIZE(args), args);
        if(res != Errors::NO_ERROR)
            exitmsg("Unable to load " << args[0]);

        code = vpe.wait();
        cout << "VPE finished with exitcode " << code << "\n";
    }

    {
        VPE vpe("hello");

        vpe.delegate(VPE::all_caps());
        if(Errors::occurred())
            exitmsg("Unable to delegate caps to VPE");

        vpe.mountspace(*VPE::self().mountspace());
        vpe.obtain_mountspace();

        vpe.fds(*VPE::self().fds());
        vpe.obtain_fds();

        int foo = 123;
        Errors::Code res = vpe.run([&mem,foo]() {
            Sync::compiler_barrier();
            cout << "myvar: " << myvar << ", foo: " << foo << "\n";
            myfunc(mem);
            return 0;
        });
        if(res != Errors::NO_ERROR)
            exitmsg("Unable to load /bin/init");

        code = vpe.wait();
        cout << "VPE finished with exitcode " << code << "\n";
    }
    return 0;
}
