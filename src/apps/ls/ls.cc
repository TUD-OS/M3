/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/Common.h>
#include <m3/stream/Serial.h>
#include <m3/util/SList.h>
#include <m3/util/Profile.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/Dir.h>
#include <m3/Log.h>

using namespace m3;

struct LSFile {
    char name[64];
    FileInfo info;
};

static void printPerm(OStream &os, mode_t mode, mode_t fl, char c) {
    if((mode & fl) != 0)
        os << c;
    else
        os << '-';
}

static void printMode(OStream &os, mode_t mode) {
    if(S_ISDIR(mode))
        os << 'd';
    else if(S_ISCHR(mode))
        os << 'c';
    else if(S_ISBLK(mode))
        os << 'b';
    else
        os << '-';
    printPerm(os, mode, S_IRUSR, 'r');
    printPerm(os, mode, S_IWUSR, 'w');
    printPerm(os, mode, S_IXUSR, 'x');
    printPerm(os, mode, S_IRGRP, 'r');
    printPerm(os, mode, S_IWGRP, 'w');
    printPerm(os, mode, S_IXGRP, 'x');
    printPerm(os, mode, S_IROTH, 'r');
    printPerm(os, mode, S_IWOTH, 'w');
    printPerm(os, mode, S_IXOTH, 'x');
}

int main(int argc, char **argv) {
    char path[256];
    if(argc < 2) {
        Serial::get() << "Usage: " << argv[0] << " <path>\n";
        return 1;
    }

    if(VFS::mount("/", new M3FS("m3fs")) < 0) {
        if(Errors::last != Errors::EXISTS)
            PANIC("Mounting root-fs failed");
    }

    cycles_t start = Profile::start(0);

    const char *dirname = argv[1];
    Dir dir(dirname);
    if(Errors::occurred())
        PANIC("open of " << dirname << " failed (" << Errors::last << ")");

    // count entries
    Dir::Entry e;
    size_t total;
    for(total = 0; dir.readdir(e); total++)
        ;

    // collect file info
    LSFile *files = new LSFile[total];
    dir.reset();
    for(size_t i = 0; dir.readdir(e); ++i) {
        OStringStream os(path, sizeof(path));
        os << dirname << "/" << e.name;
        VFS::stat(os.str(), files[i].info);
        strncpy(files[i].name, e.name, sizeof(files[i].name));
        files[i].name[sizeof(files[i].name) - 1] = '\0';
    }

    // TODO sort by name

    // print
    auto &s = Serial::get();
    for(size_t i = 0; i < total; ++i) {
        printMode(s, files[i].info.mode);
        s << ' ' << files[i].info.links << ' ' << files[i].info.size << ' ' << files[i].name << '\n';
    }

    cycles_t end = Profile::stop(0);
    Serial::get() << "Total time: " << (end - start) << " cycles\n";
    return 0;
}
