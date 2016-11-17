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

#include <base/util/String.h>
#include <base/util/BitField.h>
#include <base/EnvBackend.h>
#include <base/PEDesc.h>

#include <m3/com/RecvGate.h>

#include <pthread.h>
#include <assert.h>
#include <string>

namespace m3 {

class Env;

class HostEnvBackend : public EnvBackend {
    friend class Env;

    void exit(int) override {
    }

public:
    explicit HostEnvBackend();
    virtual ~HostEnvBackend();
};

class Env {
    struct Init {
        Init();
        ~Init();
    };
    struct PostInit {
        PostInit();
    };

public:
    static Env &get() {
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

    explicit Env(EnvBackend *backend, int logfd);
    ~Env();

    void reset();

    WorkLoop *workloop() {
        return _backend->_workloop;
    }
    EnvBackend *backend() {
        return _backend;
    }

    bool is_kernel() const {
        return pe == 0;
    }
    int log_fd() const {
        return _logfd;
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

    void init_dtu();
    void set_params(peid_t _pe, const std::string &shmprefix, label_t sysc_label,
                    epid_t sysc_ep, word_t sysc_credits) {
        pe = _pe;
        pedesc = PEDesc(PEType::COMP_IMEM, m3::PEISA::X86, 1024 * 1024);
        _shm_prefix = shmprefix.c_str();
        _sysc_label = sysc_label;
        _sysc_epid = sysc_ep;
        _sysc_credits = sysc_credits;
    }

    void exit(int code) NORETURN {
        ::exit(code);
    }

private:
    static int set_inst(Env *e) {
        _inst = e;
        // pe id
        return 0;
    }
    static void init_executable();

public:
    peid_t pe;
    PEDesc pedesc;

private:
    EnvBackend *_backend;
    int _logfd;
    String _shm_prefix;
    label_t _sysc_label;
    epid_t _sysc_epid;
    word_t _sysc_credits;
    pthread_mutex_t _log_mutex;

    static const char *_exec_short_ptr;
    static char _exec[];
    static char _exec_short[];
    static Env *_inst;
    static Init _init;
    static PostInit _postInit;
};

static inline Env *env() {
    return &Env::get();
}

}
