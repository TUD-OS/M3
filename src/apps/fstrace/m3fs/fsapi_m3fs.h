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

#pragma once

#include <base/util/Profile.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/File.h>
#include <m3/vfs/Dir.h>
#include <m3/vfs/VFS.h>

#include "common/exceptions.h"
#include "common/fsapi.h"
#include "common/buffer.h"

class FSAPI_M3FS : public FSAPI {
    enum { MaxOpenFds = 8 };

    void checkFd(int fd) {
        if(fdMap[fd] == nullptr)
            exitmsg("Using uninitialized file @ " << fd);
        fdMap[fd]->clearerr();
    }

#if defined(__gem5__)
    static cycles_t rdtsc() {
        uint32_t u, l;
        asm volatile ("rdtsc" : "=a" (l), "=d" (u) : : "memory");
        return (cycles_t)u << 32 | l;
    }
#endif

public:
    explicit FSAPI_M3FS(m3::String const &prefix)
        : _start(), _prefix(prefix), fdMap(), dirMap() {
    }

    virtual void start() override {
        _start = m3::Profile::start(0);
    }
    virtual void stop() override {
        cycles_t end = m3::Profile::stop(0);
        m3::cout << "Total time: " << (end - _start) << " cycles\n";
    }

    virtual int error() override {
        return m3::Errors::last;
    }

    virtual void checkpoint(int, int, bool) override {
        // TODO not implemented
    }

    virtual void waituntil(UNUSED const waituntil_args_t *args, int) override {
#if defined(__t2__) || defined(__t3__)
        int rem = args->timestamp / 4;
        while(rem > 0)
            asm volatile ("addi.n %0, %0, -1" : "+r"(rem));
#elif defined(__gem5__)
        cycles_t finish = rdtsc() + args->timestamp;
        while(rdtsc() < finish)
            ;
#endif
    }

    virtual void open(const open_args_t *args, UNUSED int lineNo) override {
        if(fdMap[args->fd] != nullptr || dirMap[args->fd] != nullptr)
            exitmsg("Overwriting already used file/dir @ " << args->fd);

        if(args->flags & O_DIRECTORY) {
            dirMap[args->fd] = new m3::Dir(add_prefix(args->name));
            if (dirMap[args->fd] == nullptr && args->fd >= 0)
                THROW1(ReturnValueException, m3::Errors::last, args->fd, lineNo);
        }
        else {
            fdMap[args->fd] = new m3::FStream(add_prefix(args->name), args->flags);
            if (fdMap[args->fd] == nullptr && args->fd >= 0)
                THROW1(ReturnValueException, m3::Errors::last, args->fd, lineNo);
        }
    }

    virtual void close(const close_args_t *args, int ) override {
        if(fdMap[args->fd]) {
            delete fdMap[args->fd];
            fdMap[args->fd] = nullptr;
        }
        else if(dirMap[args->fd]) {
            delete dirMap[args->fd];
            dirMap[args->fd] = nullptr;
        }
        else
            exitmsg("Using uninitialized file @ " << args->fd);
    }

    virtual void fsync(const fsync_args_t *, int ) override {
        // TODO not implemented
    }

    virtual ssize_t read(int fd, void *buffer, size_t size) override {
        checkFd(fd);
        return fdMap[fd]->read(buffer, size);
    }

    virtual ssize_t write(int fd, const void *buffer, size_t size) override {
        checkFd(fd);
        return fdMap[fd]->write(buffer, size);
    }

    virtual ssize_t pread(int fd, void *buffer, size_t size, off_t offset) override {
        checkFd(fd);
        fdMap[fd]->seek(offset, SEEK_SET);
        return fdMap[fd]->read(buffer, size);
    }

    virtual ssize_t pwrite(int fd, const void *buffer, size_t size, off_t offset) override {
        checkFd(fd);
        fdMap[fd]->seek(offset, SEEK_SET);
        return fdMap[fd]->write(buffer, size);
    }

    virtual void lseek(const lseek_args_t *args, UNUSED int lineNo) override {
        checkFd(args->fd);
        fdMap[args->fd]->seek(args->offset, args->whence);
        // if (res != args->err)
        //     THROW1(ReturnValueException, res, args->offset, lineNo);
    }

    virtual void ftruncate(const ftruncate_args_t *, int ) override {
        // TODO not implemented
    }

    virtual void fstat(const fstat_args_t *args, UNUSED int lineNo) override {
        int res;
        m3::FileInfo info;
        if(fdMap[args->fd])
            res = fdMap[args->fd]->stat(info);
        else if(dirMap[args->fd])
            res = dirMap[args->fd]->stat(info);
        else
            exitmsg("Using uninitialized file/dir @ " << args->fd);

        if ((res == m3::Errors::NO_ERROR) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void stat(const stat_args_t *args, UNUSED int lineNo) override {
        m3::FileInfo info;
        int res = m3::VFS::stat(add_prefix(args->name), info);
        if ((res == m3::Errors::NO_ERROR) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void rename(const rename_args_t *, int ) override {
        // TODO not implemented
    }

    virtual void unlink(const unlink_args_t *args, UNUSED int lineNo) override {
        int res = m3::VFS::unlink(add_prefix(args->name));
        if ((res == m3::Errors::NO_ERROR) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void rmdir(const rmdir_args_t *args, UNUSED int lineNo) override {
        int res = m3::VFS::rmdir(add_prefix(args->name));
        if ((res == m3::Errors::NO_ERROR) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void mkdir(const mkdir_args_t *args, UNUSED int lineNo) override {
        int res = m3::VFS::mkdir(add_prefix(args->name), 0777 /*args->mode*/);
        if ((res == m3::Errors::NO_ERROR) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void sendfile(Buffer &buf, const sendfile_args_t *args, int) override {
        assert(args->offset == nullptr);
        checkFd(args->in_fd);
        checkFd(args->out_fd);
        m3::FStream *in = fdMap[args->in_fd];
        m3::FStream *out = fdMap[args->out_fd];
        char *rbuf = buf.readBuffer(Buffer::MaxBufferSize);
        size_t rem = args->count;
        while(rem > 0) {
            size_t amount = m3::Math::min(static_cast<size_t>(Buffer::MaxBufferSize), rem);
            size_t res = in->read(rbuf, amount);
            out->write(rbuf, res);
            rem -= amount;
        }
    }

    virtual void getdents(const getdents_args_t *args, UNUSED int lineNo) override {
        if(dirMap[args->fd] == nullptr)
            exitmsg("Using uninitialized dir @ " << args->fd);
        m3::Dir::Entry e;
        int i;
        // we don't check the result here because strace is often unable to determine the number of
        // fetched entries.
        if(args->count == 0 && dirMap[args->fd]->readdir(e))
            ; //THROW1(ReturnValueException, 1, args->count, lineNo);
        else {
            for(i = 0; i < args->count && dirMap[args->fd]->readdir(e); ++i)
                ;
            //if(i != args->count)
            //    THROW1(ReturnValueException, i, args->count, lineNo);
        }
    }

    virtual void createfile(const createfile_args_t *, int ) override {
        // TODO not implemented
    }

private:
    const char *add_prefix(const char *path) {
        static char tmp[255];
        if(_prefix.length() == 0 || path[0] == '/')
            return path;

        m3::OStringStream os(tmp, sizeof(tmp));
        os << _prefix << path;
        return tmp;
    }

    cycles_t _start;
    const m3::String _prefix;
    m3::FStream *fdMap[MaxOpenFds];
    m3::Dir *dirMap[MaxOpenFds];
};
