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

#include <m3/Syscalls.h>
#include <m3/GateStream.h>
#include <m3/cap/Session.h>
#include <m3/service/M3FS.h>
#include <m3/stream/FStream.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/FileRef.h>
#include <m3/vfs/Dir.h>
#include <m3/Log.h>

using namespace m3;

#define TEXT "This is a test"

alignas(DTU_PKG_SIZE) static char buffer[1024 + 1];

int main() {
    if(VFS::mount("/", new M3FS("m3fs")) < 0) {
        if(Errors::last != Errors::EXISTS)
            PANIC("Mounting root-fs failed");
    }

    {
        const char *dirname = "/largedir";
        Dir dir(dirname);
        if(Errors::occurred())
            PANIC("open of " << dirname << " failed (" << Errors::last << ")");

        Serial::get() << "Listing dir " << dirname << "..." << "\n";
        Dir::Entry e;
        while(dir.readdir(e))
            Serial::get() << " Found " << e.name << " -> " << e.nodeno << "\n";
    }

    {
        const char *filename = "/BitField.h";
        FStream file(filename, FILE_RW);
        if(Errors::occurred())
            PANIC("open of " << filename << " failed (" << Errors::last << ")");

        FileInfo info;
        if(file.file().stat(info) < 0)
            PANIC("stat of " << filename << " failed");
        Serial::get() << "Info:" << "\n";
        Serial::get() << "  devno  : " << info.devno << "\n";
        Serial::get() << "  inode  : " << info.inode << "\n";
        Serial::get() << "  mode   : " << info.mode << "\n";
        Serial::get() << "  size   : " << info.size << "\n";
        Serial::get() << "  links  : " << info.links << "\n";
        Serial::get() << "  acctime: " << info.lastaccess << "\n";
        Serial::get() << "  modtime: " << info.lastmod << "\n";
        Serial::get() << "" << "\n";

        Serial::get() << "Changing content of " << filename << "..." << "\n";
        strncpy(buffer, TEXT, sizeof(TEXT) - 1);
        size_t size = sizeof(TEXT) - 1;
        if(file.write(buffer, size) != size)
            Serial::get() << "Writing failed" << "\n";

        Serial::get() << "Seeking to beginning..." << "\n";
        file.seek(0, SEEK_SET);

        Serial::get() << "Writing content of " << filename << " to stdout..." << "\n";
        ssize_t count;
        while((count = file.read(buffer, sizeof(buffer) - 1)) > 0) {
            buffer[count] = '\0';
            Serial::get() << buffer;
        }
    }

    {
        const char *dirname = "/mydir";
        Errors::Code res;
        if((res = VFS::mkdir(dirname, 0755)) != Errors::NO_ERROR)
            PANIC("mkdir(" << dirname << ") failed: " << Errors::to_string(res));
    }

    {
        const char *filename = "/mydir/foobar///";
        FStream file(filename, FILE_CREATE | FILE_W);
        if(!file)
            PANIC("open of " << filename << " failed (" << Errors::last << ")");
        file << "My test!\n";
    }

    {
        if(VFS::link("/mydir/foobar", "/mydir/here") != Errors::NO_ERROR)
            PANIC("Link failed (" << Errors::last << ")");

        if(VFS::unlink("/mydir/foobar") != Errors::NO_ERROR)
            PANIC("Unlink failed (" << Errors::last << ")");
        if(VFS::unlink("/mydir/here") != Errors::NO_ERROR)
            PANIC("Unlink failed (" << Errors::last << ")");
    }

    {
        if(VFS::rmdir("/mydir") != Errors::NO_ERROR)
            PANIC("rmdir failed (" << Errors::last << ")");
    }
    return 0;
}
