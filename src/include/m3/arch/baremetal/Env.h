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

#include <m3/Common.h>
#include <m3/Config.h>

namespace m3 {

class RecvBuf;
class RecvGate;
class OStream;

struct Env;
OStream &operator<<(OStream &, const Env &senv);

struct Env {
    friend OStream &operator<<(OStream &, const Env &senv);

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
    uint32_t mount_len;
    uintptr_t mounts;
    uintptr_t eps;
    uintptr_t caps;
    uintptr_t exit;

    RecvBuf *def_recvbuf;
    RecvGate *def_recvgate;

    static void run() asm("env_run");

private:
    void init();
    void pre_init();
    void post_init();
    void reinit();
} PACKED;

#define RT_SPACE_SIZE           (RT_SIZE - (DEF_RCVBUF_SIZE + sizeof(word_t) * 2 + sizeof(m3::Env)))
#define RT_SPACE_START          (RT_START + sizeof(m3::Env))
#define RT_SPACE_END            (RT_SPACE_START + RT_SPACE_SIZE)

static inline Env *env() {
    return reinterpret_cast<Env*>(RT_START);
}

}
