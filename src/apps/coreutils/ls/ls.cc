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
#include <base/col/SList.h>
#include <base/util/Sort.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/vfs/Dir.h>

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
    if(M3FS_ISDIR(mode))
        os << 'd';
    else if(M3FS_ISCHR(mode))
        os << 'c';
    else if(M3FS_ISBLK(mode))
        os << 'b';
    else
        os << '-';
    printPerm(os, mode, M3FS_IRUSR, 'r');
    printPerm(os, mode, M3FS_IWUSR, 'w');
    printPerm(os, mode, M3FS_IXUSR, 'x');
    printPerm(os, mode, M3FS_IRGRP, 'r');
    printPerm(os, mode, M3FS_IWGRP, 'w');
    printPerm(os, mode, M3FS_IXGRP, 'x');
    printPerm(os, mode, M3FS_IROTH, 'r');
    printPerm(os, mode, M3FS_IWOTH, 'w');
    printPerm(os, mode, M3FS_IXOTH, 'x');
}

static bool compareEntries(const LSFile &a,const LSFile &b) {
    if(M3FS_ISDIR(a.info.mode) == M3FS_ISDIR(b.info.mode))
        return strcmp(a.name, b.name) < 0;
    if(M3FS_ISDIR(a.info.mode))
        return true;
    return false;
}

int main(int argc, char **argv) {
    char path[256];
    if(argc < 2)
        exitmsg("Usage: " << argv[0] << " [-ila] <path>");

    bool longlist = false;
    bool showino = false;
    bool showall = false;
    const char *dirname = argv[argc - 1];

    for(int i = 1; i < argc - 1; ++i) {
        if(argv[i][0] != '-')
            exitmsg("Invalid arguments");

        for(size_t j = 1; argv[i][j]; ++j) {
            switch(argv[i][j]) {
                case 'l':
                    longlist = true;
                    break;
                case 'i':
                    showino = true;
                    break;
                case 'a':
                    showall = true;
                    break;
            }
        }
    }

    Errors::Code res;
    FileInfo info;
    if((res = VFS::stat(dirname, info)) != Errors::NONE)
        exitmsg("stat of " << dirname << " failed");
    if(!M3FS_ISDIR(info.mode))
        exitmsg(dirname << " is no directory");

    Dir dir(dirname);
    if(Errors::occurred())
        exitmsg("open of " << dirname << " failed");

    // count entries
    Dir::Entry e;
    size_t total;
    for(total = 0; dir.readdir(e); ) {
        if(showall || e.name[0] != '.')
            total++;
    }

    // collect file info
    LSFile *files = new LSFile[total];
    dir.reset();
    for(size_t i = 0; dir.readdir(e); ) {
        if(showall || e.name[0] != '.') {
            OStringStream os(path, sizeof(path));
            os << dirname << "/" << e.name;
            VFS::stat(os.str(), files[i].info);
            strncpy(files[i].name, e.name, sizeof(files[i].name));
            files[i].name[sizeof(files[i].name) - 1] = '\0';
            i++;
        }
    }

    // sort them
    sort(files, files + total, compareEntries);

    // print
    if(longlist) {
        for(size_t i = 0; i < total; ++i) {
            if(showino)
                cout << fmt(files[i].info.inode, 4) << ' ';
            printMode(cout, files[i].info.mode);
            cout << ' ';
            cout << files[i].info.links << ' ';
            cout << fmt(files[i].info.size, 7) << ' ';
            cout << files[i].name << '\n';
        }
    }
    else {
        for(size_t i = 0; i < total; ++i) {
            if(showino)
                cout << files[i].info.inode << ' ';
            cout << files[i].name << ' ';
        }
        cout << '\n';
    }
    return 0;
}
