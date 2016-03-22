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

#include <m3/session/Pager.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/FileRef.h>
#include <m3/vfs/RegularFile.h>
#include <m3/VPE.h>

using namespace m3;

int main() {
    cout << "Hello world!\n";

    if(VPE::self().pager()) {
        FileRef file("/BitField.h", FILE_R);
        if(Errors::last != Errors::NO_ERROR)
            exitmsg("Unable to open /BitField.h");

        FileInfo info;
        file->stat(info);
        // TODO that is not nice
        RegularFile *rfile = static_cast<RegularFile*>(&*file);
        uintptr_t virt = 0x104000;
        Errors::Code res = VPE::self().pager()->map_ds(&virt, Math::round_up(info.size, PAGE_SIZE),
            Pager::READ, 0, *rfile->fs(), rfile->fd(), 0);
        if(res != Errors::NO_ERROR)
            exitmsg("Unable to map /largetext.txt");

        const char *str = reinterpret_cast<const char*>(virt);
        cout << "Printing string at " << fmt((void*)str, "p") << ":\n";
        cout << str;
        cout << "Done\n";
    }
    return 0;
}
