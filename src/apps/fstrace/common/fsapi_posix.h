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

#include <sys/time.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <errno.h>
#include <assert.h>

#include "exceptions.h"
#include "fsapi.h"

class FSAPI_POSIX : public FSAPI {
    enum { MaxOpenFds = 512 };

public:
    explicit FSAPI_POSIX(std::string const &rootDir)
        : fdMap(new int[MaxOpenFds]),
          pathPrefix(rootDir),
          tv_start(),
          print_progress_time() {
        gettimeofday(&tv_start, nullptr);
        print_progress_time = tv_start.tv_sec * 1000000 + tv_start.tv_usec;
    }

    virtual void start() override {
    }
    virtual void stop() override {
    }

    virtual int error() override {
        return -errno;
    }

    virtual void checkpoint(int numReplayed, int numTraceOps, bool make_chkpt) override {
        struct timeval tv_iter;
        gettimeofday(&tv_iter, nullptr);
        __time_t current_time = tv_iter.tv_sec * 1000000 + tv_iter.tv_usec;
        if (current_time - print_progress_time > 30000000) {
            Platform::logf("Replayed %u of %u operations in %llu seconds so far ...\n",
                           numReplayed, numTraceOps,
                           (current_time - (tv_start.tv_sec * 1000000 + tv_start.tv_usec)) / 1000000);
            print_progress_time = current_time;
            if (make_chkpt)
                Platform::checkpoint_fs();
        }
    }

    virtual void waituntil(const waituntil_args_t *args, int) override {
        // convert to microseconds
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        __time_t now  = tv.tv_sec * 1000000 + tv.tv_usec;
        __time_t then = static_cast<__time_t>(args->timestamp) / 1000;
        static __time_t time_offset = 0;
        if (time_offset == 0) {
            time_offset = now - then;
            Platform::logf("time_offset=%llu\n", time_offset);
        }
        then += time_offset;
        if (now + 1000 < then) {
            //Platform::logf("sleeping for %llu us\n", then - now);
            usleep (then - now);
        }
    }

    virtual void open(const open_args_t *args, int lineNo) override {
        int fd = ::open(redirectPath(args->name), args->flags, 0666 /*args->mode*/);
        if (fd < 0 && args->fd >= 0)
            THROW1(ReturnValueException, -errno, args->fd, lineNo);
        else if (fd >= 0)
            fdMap[args->fd] = fd;
    }

    virtual void close(const close_args_t *args, int lineNo) override {
        int err = ::close(fdMap[args->fd]);
        if (err != args->err)
            THROW1(ReturnValueException, -errno, args->err, lineNo);
        fdMap[args->fd] = -1;
    }

    virtual void fsync(const fsync_args_t *args, int lineNo) override {
        int err = ::fsync(fdMap[args->fd]);
        if (err != args->err)
            THROW1(ReturnValueException, -errno, args->err, lineNo);
    }

    virtual ssize_t read(int fd, void *buffer, size_t size) override {
        return ::read(fdMap[fd], buffer, size);
    }

    virtual ssize_t write(int fd, const void *buffer, size_t size) override {
        return ::write(fdMap[fd], buffer, size);
    }

    virtual ssize_t pread(int fd, void *buffer, size_t size, off_t offset) override {
        return ::pread(fdMap[fd], buffer, size, offset);
    }

    virtual ssize_t pwrite(int fd, const void *buffer, size_t size, off_t offset) override {
        return ::pwrite(fdMap[fd], buffer, size, offset);
    }

    virtual void lseek(const lseek_args_t *args, int lineNo) override {
        off64_t err = ::lseek64(fdMap[args->fd], args->offset, args->whence);
        if (err != args->err)
            THROW1(ReturnValueException, err, args->offset, lineNo);
    }

    virtual void ftruncate(const ftruncate_args_t *args, int lineNo) override {
        off64_t err = ::ftruncate(fdMap[args->fd], args->offset);
        if (err != args->err)
            THROW1(ReturnValueException, err, args->offset, lineNo);
    }

    virtual void fstat(const fstat_args_t *args, int lineNo) override {
        struct stat st;
        off64_t err = ::fstat(fdMap[args->fd], &st);
        if (err != args->err)
            THROW1(ReturnValueException, err, args->err, lineNo);
    }

    virtual void fstatat(const fstatat_args_t *args, int lineNo) override {
        struct stat st;
        off64_t err = ::stat(redirectPath(args->name), &st);
        if (err != args->err)
            THROW1(ReturnValueException, err, args->err, lineNo);
    }

    virtual void stat(const stat_args_t *args, int lineNo) override {
        struct stat st;
        off64_t err = ::stat(redirectPath(args->name), &st);
        if (err != args->err)
            THROW1(ReturnValueException, err, args->err, lineNo);
    }

    virtual void rename(const rename_args_t *args, int lineNo) override {
        int err = ::rename(redirectPath(args->from), redirectPath(args->to));
        if (err != args->err)
            THROW1(ReturnValueException, -errno, args->err, lineNo);
    }

    virtual void unlink(const unlink_args_t *args, int lineNo) override {
        int err = ::unlink(redirectPath(args->name));
        if (err != args->err)
            THROW1(ReturnValueException, -errno, args->err, lineNo);
    }

    virtual void rmdir(const rmdir_args_t *args, int lineNo) override {
        int err = ::rmdir(redirectPath(args->name));
        if (err != args->err)
            THROW1(ReturnValueException, -errno, args->err, lineNo);
    }

    virtual void mkdir(const mkdir_args_t *args, int lineNo) override {
        int err = ::mkdir(redirectPath(args->name), 0777 /*args->mode*/);
        if (err != args->err && errno != EEXIST)
            THROW1(ReturnValueException, -errno, args->err, lineNo);
    }

    virtual void sendfile(Buffer &, const sendfile_args_t *args, int lineNo) override {
        assert(args->offset == nullptr);
        int err = ::sendfile64(args->out_fd, args->in_fd, nullptr, args->count);
        if (err != args->err)
            THROW1(ReturnValueException, -errno, args->err, lineNo);
    }

    virtual void getdents(const getdents_args_t *args, int lineNo) override {
        struct linux_dirent {
            long           d_ino;
            off_t          d_off;
            unsigned short d_reclen;
            char           d_name[];
        };
        char buffer[args->bufsize];
        int nread = syscall(SYS_getdents, args->fd, buffer, args->bufsize);
        if(nread != args->err)
            THROW1(ReturnValueException, nread, args->err, lineNo);
    }

    virtual void createfile(const createfile_args_t *args, int lineNo) override {
        int flags = O_RDWR | O_CREAT;
        int err = 0;
        int fd = ::open(redirectPath(args->name), flags, args->mode);

        //Platform::logf("0: fd=%d errno=%d\n", fd, -errno);
        off64_t new_pos = 0;
        struct stat64 st;
        err = fstat64(fd, &st);

        // try to optimize writes, if the file already exists
        if (err == 0)
            new_pos = (st.st_size >> 11) << 11;
    #if DEBUG_BENCH_SPARSE_FILES
        new_pos = args->size;
    #endif

        while (err == 0 && fd >= 0 && new_pos < args->size + 4096) {
            off64_t at_pos;
            off64_t to_pos = new_pos;
            if (to_pos >= args->size)
                to_pos = args->size - 1;
            at_pos = ::lseek64(fd, to_pos, SEEK_SET);
            err += (to_pos != at_pos) ? -1 : 0;
            //Platform::logf("1: err=%d\n", err);
            err += (::write(fd, &fd, 1) != 1) ? -1 : 0;
            //Platform::logf("2: err=%d\n", err);
            new_pos += 4096;
        }

        // if the file existed already., it might be too big
        if (err == 0 && fd >= 0)
            err += ::ftruncate(fd, args->size);
        err += ::close(fd);

        //Platform::logf("3: err=%d\n", err);
        if (fd < 0 || err != 0)
            THROW1(ReturnValueException, -errno, 0, lineNo);
    }

    virtual void accept(const accept_args_t *, int) override {
        // TODO unsupported
    }
    virtual void recvfrom(Buffer &, const recvfrom_args_t *, int) override {
        // TODO unsupported
    }
    virtual void writev(Buffer &, const writev_args_t *, int) override {
        // TODO unsupported
    }

private:
    char const *redirectPath(char const *path) {
        static unsigned cur_buf_idx = 0;
        static char pathBuf[2][2048];

        cur_buf_idx++;
        if (cur_buf_idx >= 2)
            cur_buf_idx = 0;

        strcpy(pathBuf[cur_buf_idx], pathPrefix.c_str());
        strncat(pathBuf[cur_buf_idx], path, sizeof(pathBuf) - pathPrefix.size());
        return pathBuf[cur_buf_idx];
    }

    int *fdMap;
    std::string pathPrefix;
    struct timeval tv_start;
    __time_t print_progress_time;
};
