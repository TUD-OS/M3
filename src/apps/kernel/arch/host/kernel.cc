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

#include <base/col/SList.h>
#include <base/log/Kernel.h>
#include <base/Config.h>
#include <base/DTU.h>
#include <base/Panic.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#include "pes/PEManager.h"
#include "pes/VPEManager.h"
#include "pes/VPE.h"
#include "dev/TimerDevice.h"
#include "dev/VGAConsole.h"
#include "SyscallHandler.h"

using namespace kernel;

static m3::SList<Device> devices;
static size_t fssize = 0;

static void sigint(int) {
    m3::env()->workloop()->stop();
}

static void delete_dir(const char *dir) {
    char path[64];
    DIR *d = opendir(dir);
    struct dirent *e;
    while((e = readdir(d))) {
        if(strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        unlink(path);
    }
    closedir(d);
    rmdir(dir);
}

static void copyfromfs(MainMemory &mem, const char *file) {
    int fd = open(file, O_RDONLY);
    if(fd < 0)
        PANIC("Opening '" << file << "' for reading failed");

    struct stat info;
    if(fstat(fd, &info) == -1)
        PANIC("Stat for '" << file << "' failed");
    if(info.st_size > FS_MAX_SIZE) {
        PANIC("Filesystem image (" << file << ") too large"
         " (max=" << FS_MAX_SIZE << ", size=" << info.st_size << ")");
    }

    MainMemory::Allocation alloc = mem.allocate_at(FS_IMG_OFFSET, FS_MAX_SIZE);
    ssize_t res = read(fd, reinterpret_cast<void*>(alloc.addr), alloc.size);
    if(res == -1)
        PANIC("Reading from '" << file << "' failed");
    close(fd);

    fssize = static_cast<size_t>(res);
    KLOG(MEM, "Copied fs-image '" << file << "' to 0.." << m3::fmt(fssize, "#x"));
}

static void copytofs(MainMemory &mem, const char *file) {
    char name[256];
    snprintf(name, sizeof(name), "%s.out", file);
    int fd = open(name, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if(fd < 0)
        PANIC("Opening '" << name << "' for writing failed");

    MainMemory::Allocation alloc = mem.allocate_at(FS_IMG_OFFSET, FS_MAX_SIZE);
    write(fd, reinterpret_cast<void*>(alloc.addr), fssize);
    close(fd);

    KLOG(MEM, "Copied fs-image from memory back to '" << name << "'");
}

int main(int argc, char *argv[]) {
    const char *fsimg = nullptr;
    mkdir("/tmp/m3", 0755);
    signal(SIGINT, sigint);

    for(int i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "-c") == 0)
            devices.append(new VGAConsoleDevice());
        else if(strcmp(argv[i], "-t") == 0)
            devices.append(new TimerDevice());
        else if(strncmp(argv[i], "fs=", 3) == 0)
            fsimg = argv[i] + 3;
    }

    int argstart = 0;
    for(int i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "--") == 0) {
            argstart = i;
            break;
        }
    }

    KLOG(MEM, MainMemory::get());

    // create some worker threads
    m3::env()->workloop()->multithreaded(8);

    if(fsimg)
        copyfromfs(MainMemory::get(), fsimg);
    SyscallHandler::init();
    PEManager::create();
    VPEManager::create();
    VPEManager::get().init(argc - argstart - 1, argv + argstart + 1);
    PEManager::get().init();

    KLOG(INFO, "Kernel is ready");

    m3::env()->workloop()->run();

    KLOG(INFO, "Shutting down");
    if(fsimg)
        copytofs(MainMemory::get(), fsimg);
    VPEManager::destroy();
    for(auto it = devices.begin(); it != devices.end(); ) {
        auto old = it++;
        old->stop();
        delete &*old;
    }
    delete_dir("/tmp/m3");
    return EXIT_SUCCESS;
}
