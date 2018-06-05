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

#include <base/util/Time.h>

#include <m3/stream/Standard.h>
#include <m3/vfs/File.h>
#include <m3/vfs/Dir.h>
#include <m3/vfs/VFS.h>
#include <m3/VPE.h>

#include "common/exceptions.h"
#include "common/fsapi.h"
#include "common/buffer.h"

class FSAPI_M3FS : public FSAPI {
    enum { MaxOpenFds = 8 };

    void checkFd(int fd) {
        if(_fdMap[fd] == 0)
            exitmsg("Using uninitialized file @ " << fd);
    }

public:
    explicit FSAPI_M3FS(bool wait, m3::String const &prefix)
        : _wait(wait),
          _start(),
          _prefix(prefix),
          _fdMap(),
          _dirMap() {
    }

    virtual void start() override {
        _start = m3::Time::start(0);
    }
    virtual void stop() override {
        cycles_t end = m3::Time::stop(0);
        m3::cout << "Total time: " << (end - _start) << " cycles\n";
    }

    virtual int error() override {
        return m3::Errors::last;
    }

    virtual void checkpoint(int, int, bool) override {
        // TODO not implemented
    }

    virtual void waituntil(UNUSED const waituntil_args_t *args, int) override {
        if(_wait)
            m3::CPU::compute(args->timestamp);
    }

    virtual void open(const open_args_t *args, UNUSED int lineNo) override {
        if(args->fd != -1 && (_fdMap[args->fd] != 0 || _dirMap[args->fd] != nullptr))
            exitmsg("Overwriting already used file/dir @ " << args->fd);

        if(args->flags & O_DIRECTORY) {
            auto dir = new m3::Dir(add_prefix(args->name));
            if(m3::Errors::occurred()) {
               delete dir;
                if(args->fd != -1)
                   THROW1(ReturnValueException, m3::Errors::last, args->fd, lineNo);
            }
            else {
                _dirMap[args->fd] = dir;
                if (_dirMap[args->fd] == nullptr && args->fd >= 0)
                    THROW1(ReturnValueException, m3::Errors::last, args->fd, lineNo);
            }
        }
        else {
            auto nfile = m3::VFS::open(add_prefix(args->name), args->flags | m3::FILE_NODATA);
            if(m3::Errors::occurred()) {
                m3::VFS::close(nfile);
                if(args->fd != -1)
                   THROW1(ReturnValueException, m3::Errors::last, args->fd, lineNo);
            }
            else {
                _fdMap[args->fd] = nfile;
                if(_fdMap[args->fd] == m3::FileTable::INVALID)
                    THROW1(ReturnValueException, m3::Errors::last, args->fd, lineNo);
            }
        }
    }

    virtual void close(const close_args_t *args, int ) override {
        if(_fdMap[args->fd]) {
            m3::VFS::close(_fdMap[args->fd]);
            _fdMap[args->fd] = 0;
        }
        else if(_dirMap[args->fd]) {
            delete _dirMap[args->fd];
            _dirMap[args->fd] = nullptr;
        }
        else
            exitmsg("Using uninitialized file @ " << args->fd);
    }

    virtual void fsync(const fsync_args_t *, int ) override {
        // TODO not implemented
    }

    virtual ssize_t read(int fd, void *buffer, size_t size) override {
        checkFd(fd);
        m3::File *file = m3::VPE::self().fds()->get(_fdMap[fd]);
        char *buf = reinterpret_cast<char*>(buffer);
        while(size > 0) {
            ssize_t res = file->read(buf, size);
            if(res < 0)
                return m3::Errors::last;
            if(res == 0)
                break;
            size -= static_cast<size_t>(res);
            buf += res;
        }
        return buf - reinterpret_cast<char*>(buffer);
    }

    virtual ssize_t write(int fd, const void *buffer, size_t size) override {
        checkFd(fd);
        m3::File *file = m3::VPE::self().fds()->get(_fdMap[fd]);
        return write_file(file, buffer, size);
    }

    ssize_t write_file(m3::File *file, const void *buffer, size_t size) {
        m3::Errors::Code res = file->write_all(buffer, size);
        if(res != m3::Errors::NONE)
            return -static_cast<ssize_t>(res);
        return static_cast<ssize_t>(size);
    }

    virtual ssize_t pread(int fd, void *buffer, size_t size, off_t offset) override {
        checkFd(fd);
        m3::VPE::self().fds()->get(_fdMap[fd])->seek(static_cast<size_t>(offset), M3FS_SEEK_SET);
        return read(fd, buffer, size);
    }

    virtual ssize_t pwrite(int fd, const void *buffer, size_t size, off_t offset) override {
        checkFd(fd);
        m3::VPE::self().fds()->get(_fdMap[fd])->seek(static_cast<size_t>(offset), M3FS_SEEK_SET);
        return write(fd, buffer, size);
    }

    virtual void lseek(const lseek_args_t *args, UNUSED int lineNo) override {
        checkFd(args->fd);
        m3::VPE::self().fds()->get(_fdMap[args->fd])->seek(static_cast<size_t>(args->offset), args->whence);
        // if (res != args->err)
        //     THROW1(ReturnValueException, res, args->offset, lineNo);
    }

    virtual void ftruncate(const ftruncate_args_t *, int ) override {
        // TODO not implemented
    }

    virtual void fstat(const fstat_args_t *args, UNUSED int lineNo) override {
        int res;
        m3::FileInfo info;
        if(_fdMap[args->fd])
            res = m3::VPE::self().fds()->get(_fdMap[args->fd])->stat(info);
        else if(_dirMap[args->fd])
            res = _dirMap[args->fd]->stat(info);
        else
            exitmsg("Using uninitialized file/dir @ " << args->fd);

        if ((res == m3::Errors::NONE) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void fstatat(const fstatat_args_t *args, UNUSED int lineNo) override {
        m3::FileInfo info;
        int res = m3::VFS::stat(add_prefix(args->name), info);
        if ((res == m3::Errors::NONE) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void stat(const stat_args_t *args, UNUSED int lineNo) override {
        m3::FileInfo info;
        int res = m3::VFS::stat(add_prefix(args->name), info);
        if ((res == m3::Errors::NONE) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void rename(const rename_args_t *args, int lineNo) override {
        int res = m3::VFS::link(args->from, args->to);
        if ((res == m3::Errors::NONE) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
        res = m3::VFS::unlink(args->from);
        if ((res == m3::Errors::NONE) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void unlink(const unlink_args_t *args, UNUSED int lineNo) override {
        int res = m3::VFS::unlink(add_prefix(args->name));
        if ((res == m3::Errors::NONE) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void rmdir(const rmdir_args_t *args, UNUSED int lineNo) override {
        int res = m3::VFS::rmdir(add_prefix(args->name));
        if ((res == m3::Errors::NONE) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void mkdir(const mkdir_args_t *args, UNUSED int lineNo) override {
        int res = m3::VFS::mkdir(add_prefix(args->name), 0777 /*args->mode*/);
        if ((res == m3::Errors::NONE) != (args->err == 0))
            THROW1(ReturnValueException, res, args->err, lineNo);
    }

    virtual void sendfile(Buffer &buf, const sendfile_args_t *args, int lineNo) override {
        assert(args->offset == nullptr);
        checkFd(args->in_fd);
        checkFd(args->out_fd);
        m3::File *in = m3::VPE::self().fds()->get(_fdMap[args->in_fd]);
        m3::File *out = m3::VPE::self().fds()->get(_fdMap[args->out_fd]);
        char *rbuf = buf.readBuffer(Buffer::MaxBufferSize);
        size_t rem = args->count;
        while(rem > 0) {
            size_t amount = m3::Math::min(static_cast<size_t>(Buffer::MaxBufferSize), rem);

            ssize_t res = in->read(rbuf, amount);
            if(res < 0)
                THROW1(ReturnValueException, res, amount, lineNo);

            ssize_t wres = write_file(out, rbuf, static_cast<size_t>(res));
            if(wres != res)
                THROW1(ReturnValueException, wres, res, lineNo);

            rem -= static_cast<size_t>(res);
        }
    }

    virtual void getdents(const getdents_args_t *args, UNUSED int lineNo) override {
        if(_dirMap[args->fd] == nullptr)
            exitmsg("Using uninitialized dir @ " << args->fd);
        m3::Dir::Entry e;
        int i;
        // we don't check the result here because strace is often unable to determine the number of
        // fetched entries.
        if(args->count == 0 && _dirMap[args->fd]->readdir(e))
            ; //THROW1(ReturnValueException, 1, args->count, lineNo);
        else {
            for(i = 0; i < args->count && _dirMap[args->fd]->readdir(e); ++i)
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
        if(_prefix.length() == 0 || strncmp(path, "/tmp/", 5) != 0)
            return path;

        m3::OStringStream os(tmp, sizeof(tmp));
        os << _prefix << (path + 5);
        return tmp;
    }

    bool _wait;
    cycles_t _start;
    const m3::String _prefix;
    fd_t _fdMap[MaxOpenFds];
    m3::Dir *_dirMap[MaxOpenFds];
};
