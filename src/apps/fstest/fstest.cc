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

#include <m3/com/GateStream.h>
#include <m3/session/Session.h>
#include <m3/session/M3FS.h>
#include <m3/stream/FStream.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/vfs/Dir.h>

using namespace m3;

#define TEXT "This is a test"

alignas(DTU_PKG_SIZE) static char buffer[1024 + 1];

int main() {
    if(VFS::mount("/", new M3FS("m3fs")) < 0) {
        if(Errors::last != Errors::EXISTS)
            exitmsg("Mounting root-fs failed");
    }

    {
        const char *dirname = "/largedir";
        Dir dir(dirname);
        if(Errors::occurred())
            exitmsg("open of " << dirname << " failed");

        cout << "Listing dir " << dirname << "..." << "\n";
        Dir::Entry e;
        while(dir.readdir(e))
            cout << " Found " << e.name << " -> " << e.nodeno << "\n";
    }

    {
        const char *filename = "/BitField.h";
        FStream file(filename, FILE_RW);
        if(Errors::occurred())
            exitmsg("open of " << filename << " failed");

        FileInfo info;
        if(file.file()->stat(info) < 0)
            exitmsg("stat of " << filename << " failed");
        cout << "Info:" << "\n";
        cout << "  devno  : " << info.devno << "\n";
        cout << "  inode  : " << info.inode << "\n";
        cout << "  mode   : " << info.mode << "\n";
        cout << "  size   : " << info.size << "\n";
        cout << "  links  : " << info.links << "\n";
        cout << "  acctime: " << info.lastaccess << "\n";
        cout << "  modtime: " << info.lastmod << "\n";
        cout << "" << "\n";

        cout << "Changing content of " << filename << "..." << "\n";
        strncpy(buffer, TEXT, sizeof(TEXT) - 1);
        size_t size = sizeof(TEXT) - 1;
        if(file.write(buffer, size) != size)
            cout << "Writing failed" << "\n";

        cout << "Seeking to beginning..." << "\n";
        file.seek(0, SEEK_SET);

        cout << "Writing content of " << filename << " to stdout..." << "\n";
        ssize_t count;
        while((count = file.read(buffer, sizeof(buffer) - 1)) > 0) {
            buffer[count] = '\0';
            cout << buffer;
        }
    }

    {
        const char *dirname = "/mydir";
        Errors::Code res;
        if((res = VFS::mkdir(dirname, 0755)) != Errors::NO_ERROR)
            exitmsg("mkdir(" << dirname << ") failed: " << Errors::to_string(res));
    }

    {
        const char *filename = "/mydir/foobar///";
        FStream file(filename, FILE_CREATE | FILE_W);
        if(!file)
            exitmsg("open of " << filename << " failed");
        file << "My test!\n";
    }

    {
        if(VFS::link("/mydir/foobar", "/mydir/here") != Errors::NO_ERROR)
            exitmsg("Link failed");

        if(VFS::unlink("/mydir/foobar") != Errors::NO_ERROR)
            exitmsg("Unlink failed");
        if(VFS::unlink("/mydir/here") != Errors::NO_ERROR)
            exitmsg("Unlink failed");
    }

    {
        if(VFS::rmdir("/mydir") != Errors::NO_ERROR)
            exitmsg("rmdir failed");
    }
    return 0;
}
