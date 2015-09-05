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

#pragma once

#include <m3/util/String.h>
#include <m3/BitField.h>
#include <m3/RecvBuf.h>
#include <pthread.h>
#include <assert.h>

namespace m3 {

#define MEMORY_CORE         0
#define KERNEL_CORE         0
#define APP_CORES           1
#define MAX_CORES           18
#define AVAIL_PES           (MAX_CORES - 1)
#define CAP_TOTAL           128
#define FS_IMG_OFFSET       0x0

// leave the first 64 MiB for the filesystem
#define DRAM_OFFSET         0
#define DRAM_SIZE           (512 * 1024 * 1024)

class RecvGate;

class Config {
    struct Init {
        Init();
        ~Init();
    };

public:
    static Config &get() {
        assert(_inst != nullptr);
        return *_inst;
    }

    static const char *executable_path() {
        if(*_exec == '\0')
            init_executable();
        return _exec;
    }
    static const char *executable() {
        if(_exec_short_ptr == nullptr)
            init_executable();
        return _exec_short_ptr;
    }

    explicit Config();
    explicit Config(int core, const char *shmprefix);
    ~Config();

    void reset();

    RecvGate *mem_rcvgate() {
        return _mem_recvgate;
    }
    RecvGate *def_rcvgate() {
        return _def_recvgate;
    }
    bool is_kernel() const {
        return _is_kernel;
    }
    int log_fd() const {
        return _logfd;
    }
    int core() const {
        return _core;
    }
    void log_lock() {
        pthread_mutex_lock(&_log_mutex);
    }
    void log_unlock() {
        pthread_mutex_unlock(&_log_mutex);
    }
    const String &shm_prefix() const {
        return _shm_prefix;
    }
    void print() const;

private:
    void init();
    void init_dtu();
    static bool set_params(Config *env, const char *shm_prefix, bool is_kernel);
    static void init_executable();

    int _core;
    int _logfd;
    String _shm_prefix;
    label_t _sysc_label;
    size_t _sysc_epid;
    word_t _sysc_credits;
    bool _is_kernel;
    pthread_mutex_t _log_mutex;
    RecvBuf _mem_recvbuf;
    RecvBuf _def_recvbuf;
    RecvGate *_mem_recvgate;
    RecvGate *_def_recvgate;
    static const char *_exec_short_ptr;
    static char _exec[];
    static char _exec_short[];
    static Config *_inst;
    static Init _init;
};

static inline RecvGate *def_rcvgate() {
    return Config::get().def_rcvgate();
}

static inline int coreid() {
    return Config::get().core();
}

}
