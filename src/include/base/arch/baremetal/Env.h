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

#include <base/Common.h>
#include <base/Config.h>
#include <base/EnvBackend.h>

namespace m3 {

class OStream;

class Env;
OStream &operator<<(OStream &, const Env &senv);

class BaremetalEnvBackend : public EnvBackend {
    friend class Env;

public:
    virtual void init() = 0;
    virtual void reinit() = 0;
};

class Env {
    friend OStream &operator<<(OStream &, const Env &senv);

public:
    static const size_t MODS_MAX    = 8;

    uint64_t coreid;
    uint32_t argc;
    char **argv;
    uintptr_t mods[MODS_MAX];

    uintptr_t sp;
    uintptr_t entry;
    uintptr_t lambda;
    uint32_t pager_sess;
    uint32_t pager_gate;
    uint32_t mounts_len;
    uintptr_t mounts;
    uint32_t fds_len;
    uintptr_t fds;
    uintptr_t eps;
    uintptr_t caps;
    uintptr_t exitaddr;

    BaremetalEnvBackend *backend;
#if defined(__t2__) || defined(__t3__)
    uint32_t : 32;
#endif

    WorkLoop *workloop() {
        return backend->_workloop;
    }

    static void run() asm("env_run");

    void exit(int code) NORETURN;

private:
    void jmpto(uintptr_t addr) NORETURN;
    void pre_init();
    void post_init();
    void pre_exit();
} PACKED;

#if !defined(__gem5__)
#   define RT_SPACE_SIZE           (RT_SIZE - (DEF_RCVBUF_SIZE + sizeof(word_t) * 2 + sizeof(m3::Env)))
#else
#   define RT_SPACE_SIZE           (RT_SIZE - (sizeof(word_t) * 2 + sizeof(m3::Env)))
#endif
#define RT_SPACE_START          (RT_START + sizeof(m3::Env))
#define RT_SPACE_END            (RT_SPACE_START + RT_SPACE_SIZE)

static inline Env *env() {
    return reinterpret_cast<Env*>(RT_START);
}

}
