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
#include <base/stream/Serial.h>
#include <base/util/Sync.h>
#include <base/Log.h>

#include <m3/com/MemGate.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/File.h>
#include <m3/vfs/Dir.h>
#include <m3/VPE.h>

using namespace m3;

static int myvar = 0;

static int myfunc(MemGate &mem) {
    Serial::get() << "myvar: " << myvar << "\n";

    alignas(DTU_PKG_SIZE) char buffer[16];
    mem.read_sync(buffer, sizeof(buffer), 0);
    for(size_t i = 0; i < sizeof(buffer); ++i)
        Serial::get() << i << ": " << fmt(buffer[i], "#02x") << "\n";

    const char *dirname = "/bin";
    Dir dir(dirname);
    if(Errors::occurred())
        PANIC("open of " << dirname << " failed (" << Errors::last << ")");

    Serial::get() << "Listing dir " << dirname << "...\n";
    Dir::Entry e;
    while(dir.readdir(e))
        Serial::get() << " Found " << e.name << " -> " << e.nodeno << "\n";
    return 0;
}

int main() {
    if(VFS::mount("/", new M3FS("m3fs")) < 0) {
        if(Errors::last != Errors::EXISTS)
            PANIC("Mounting root-fs failed");
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
        vpe.delegate_mounts();

        const char *args[] = {"/bin/hello", "foo", "bar"};
        Errors::Code res = vpe.exec(ARRAY_SIZE(args), args);
        if(res != Errors::NO_ERROR)
            PANIC("Unable to load " << args[0] << ": " << Errors::to_string(res));

        code = vpe.wait();
        Serial::get() << "VPE finished with exitcode " << code << "\n";
    }

    {
        VPE vpe("hello");
        vpe.delegate(VPE::all_caps());
        if(Errors::occurred())
            PANIC("Unable to delegate caps to VPE: " << Errors::to_string(Errors::last));

        int foo = 123;
        Errors::Code res = vpe.run([&mem,foo]() {
            Sync::compiler_barrier();
            Serial::get() << "myvar: " << myvar << ", foo: " << foo << "\n";
            myfunc(mem);
            return 0;
        });
        if(res != Errors::NO_ERROR)
            PANIC("Unable to load /bin/init: " << Errors::to_string(res));

        code = vpe.wait();
        Serial::get() << "VPE finished with exitcode " << code << "\n";
    }
    return 0;
}
