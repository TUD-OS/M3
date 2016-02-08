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

#include <m3/cap/Session.h>
#include <m3/cap/VPE.h>
#include <m3/service/Pager.h>
#include <m3/service/M3FS.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/vfs/RegularFile.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>

using namespace m3;

int main() {
    Serial::get() << "Hello World!\n";

    uintptr_t virt = 0x100000;
    {
        Pager pg("pager", VPE::self());
        Syscalls::get().setpfgate(VPE::self().sel(), pg.gate().sel(), VPE::self().alloc_ep());

        Errors::Code res = pg.map_anon(&virt, 0x4000, Pager::READ | Pager::WRITE, 0);
        if(res != Errors::NO_ERROR)
            PANIC("Unable to map memory:" << Errors::to_string(res));

        volatile uint64_t *pte = reinterpret_cast<volatile uint64_t*>(0x3ffff000);
        Serial::get() << fmt(*pte, "#0x", 16) << "\n";

        volatile int *addr = reinterpret_cast<volatile int*>(virt);
        for(size_t i = 0; i < (4 * PAGE_SIZE) / sizeof(int); ++i)
            addr[i] = i;

        Serial::get() << fmt(*pte, "#0x", 16) << "\n";
        Serial::get() << fmt(*pte, "#0x", 16) << "\n";

        res = pg.unmap(virt);
        if(res != Errors::NO_ERROR)
            PANIC("Unable to unmap memory:" << Errors::to_string(res));
    }

    // TODO since we do not revoke the mappings yet, this does still work
    volatile int *addr = reinterpret_cast<volatile int*>(virt);
    addr[0] = 1;

    Serial::get() << "Running child...\n";

    {
        if(VFS::mount("/", new M3FS("m3fs")) < 0) {
            if(Errors::last != Errors::EXISTS)
                PANIC("Mounting root-fs failed");
        }

        VPE child("child");
        Pager pg("pager", child);
        child.delegate_obj(pg.sel());
        child.delegate_obj(pg.gate().sel());
        Syscalls::get().setpfgate(child.sel(), pg.gate().sel(), child.alloc_ep());

        Errors::Code res = pg.map_anon(&virt, 0x4000, Pager::READ | Pager::WRITE, 0);
        if(res != Errors::NO_ERROR)
            PANIC("Unable to map memory:" << Errors::to_string(res));

        FileRef file("/largetext.txt", FILE_R);
        FileInfo info;
        file->stat(info);
        // TODO that is not nice
        RegularFile *rfile = static_cast<RegularFile*>(&*file);
        virt = 0x104000;
        res = pg.map_ds(&virt, Math::round_up(info.size, PAGE_SIZE), Pager::READ, 0,
            *rfile->fs(), rfile->fd(), 0);
        if(res != Errors::NO_ERROR)
            PANIC("Unable to map /largetext.txt:" << Errors::to_string(res));

        char buf[16];
        file->read(buf, sizeof(buf));
        Serial::get() << "Buf: " << buf << "\n";

        const char *args[] = {"/bin/pgchild"};
        child.exec(ARRAY_SIZE(args), args);
        child.wait();
    }

    Serial::get() << "Bye World!\n";
    return 0;
}
