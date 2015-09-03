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

#include <m3/cap/VPE.h>
#include <m3/Syscalls.h>
#include <m3/Config.h>
#include <m3/stream/FStream.h>
#include <m3/vfs/FileRef.h>
#include <m3/Log.h>
#include <m3/ELF.h>

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

namespace m3 {

static void write_file(pid_t pid, const char *suffix, const void *data, size_t size) {
    if(data) {
        char path[64];
        snprintf(path, sizeof(path), "/tmp/m3/%d-%s", pid, suffix);
        int fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0600);
        if(fd < 0)
            perror("open");
        else {
            write(fd, data, size);
            close(fd);
        }
    }
}

static void *read_from(const char *suffix, void *dst, size_t &size) {
    char path[64];
    snprintf(path, sizeof(path), "/tmp/m3/%d-%s", getpid(), suffix);
    int fd = open(path, O_RDONLY);
    if(fd >= 0) {
        if(dst == nullptr) {
            struct stat st;
            if(fstat(fd, &st) == -1)
                return nullptr;
            size = st.st_size;
            dst = Heap::alloc(size);
        }

        read(fd, dst, size);
        unlink(path);
        close(fd);
    }
    return dst;
}

void VPE::init_state() {
    delete _chans;
    delete _caps;
    Heap::free(_mounts);

    _caps = new BitField<CAP_TOTAL>();
    size_t len = sizeof(*_caps);
    read_from("caps", _caps, len);

    _chans = new BitField<CHAN_COUNT>();
    len = sizeof(*_chans);
    read_from("chans", _chans, len);

    _mounts = read_from("mounts", nullptr, _mountlen);
}

Errors::Code VPE::run(void *lambda) {
    char byte = 1;
    int fd[2];
    if(pipe(fd) == -1)
        return Errors::OUT_OF_MEM;

    int pid = fork();
    if(pid == -1) {
        close(fd[1]);
        close(fd[0]);
        return Errors::OUT_OF_MEM;
    }
    else if(pid == 0) {
        // child
        close(fd[1]);

        // wait until parent notifies us
        read(fd[0], &byte, 1);
        close(fd[0]);

        Config::get().reset();
        VPE::self().init_state();

        std::function<int()> *func = reinterpret_cast<std::function<int()>*>(lambda);
        (*func)();
        exit(0);
    }
    else {
        // parent
        close(fd[0]);

        // let the kernel create the config-file etc. for the given pid
        Syscalls::get().vpectrl(sel(), Syscalls::VCTRL_START, pid, nullptr);

        write_file(pid, "caps", _caps, sizeof(*_caps));
        write_file(pid, "chans", _chans, sizeof(*_chans));
        write_file(pid, "mounts", _mounts, _mountlen);

        // notify child; it can start now
        write(fd[1], &byte, 1);
        close(fd[1]);
    }
    return Errors::NO_ERROR;
}

Errors::Code VPE::exec(int argc, const char **argv) {
    static char buffer[1024];
    char templ[] = "/tmp/m3-XXXXXX";
    int tmp, pid, fd[2];
    ssize_t res;
    char byte = 1;
    if(pipe(fd) == -1)
        return Errors::OUT_OF_MEM;

    FileRef exec(argv[0], FILE_R);
    if(Errors::occurred())
        goto errorTemp;
    tmp = mkstemp(templ);
    if(tmp < 0)
        goto errorTemp;

    // copy executable from M3-fs to a temp file
    while((res = exec->read(buffer, sizeof(buffer))) > 0)
        write(tmp, buffer, res);

    pid = fork();
    if(pid == -1)
        goto errorExec;
    else if(pid == 0) {
        // child
        close(fd[1]);

        // wait until parent notifies us
        read(fd[0], &byte, 1);
        close(fd[0]);

        // copy args to null-terminate them
        char **args = new char*[argc + 1];
        for(int i = 0; i < argc; ++i)
            args[i] = (char*)argv[i];
        args[argc] = nullptr;

        // open it readonly again as fexecve requires
        int tmpdup = open(templ, O_RDONLY);
        // we don't need it anymore afterwards
        unlink(templ);
        // it needs to be executable
        fchmod(tmpdup, 0700);
        // close writable fd to make it non-busy
        close(tmp);

        // execute that file
        fexecve(tmpdup, args, environ);
        PANIC("Exec of '" << argv[0] << "' failed: " << strerror(errno));
    }
    else {
        // parent
        close(fd[0]);
        close(tmp);

        // let the kernel create the config-file etc. for the given pid
        Syscalls::get().vpectrl(sel(), Syscalls::VCTRL_START, pid, nullptr);

        write_file(pid, "caps", _caps, sizeof(*_caps));
        write_file(pid, "chans", _chans, sizeof(*_chans));
        write_file(pid, "mounts", _mounts, _mountlen);

        // notify child; it can start now
        write(fd[1], &byte, 1);
        close(fd[1]);
    }
    return Errors::NO_ERROR;

errorExec:
    close(tmp);
errorTemp:
    close(fd[0]);
    close(fd[1]);
    return Errors::OUT_OF_MEM;

}

}
