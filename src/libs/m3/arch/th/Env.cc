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

#include <m3/Common.h>
#include <m3/util/Math.h>
#include <m3/Env.h>
#include <m3/Log.h>
#include <string.h>
#include <assert.h>

#include <xtensa/simcall.h>

extern void *_bss_table_start;
extern void *_bss_table_end;

EXTERN_C void *_Exception;
static void *dummy[1];

EXTERN_C void _init();
EXTERN_C void sim_init(uint32_t *argc, char ***argv);

namespace m3 {

#if defined(__t3__)
static void get_args(Env *e) {
    size_t size;
    {
        register word_t a2 __asm__ ("a2") = SYS_iss_argv_size;
        asm volatile ("simcall" : "+a"(a2));
        size = Math::round_up(a2, static_cast<word_t>(16));
    }

    {
        register word_t a2 __asm__ ("a2") = SYS_iss_argc;
        asm volatile ("simcall" : "+a"(a2));

        e->argc = a2;
    }

    if(size + sizeof(void*) * e->argc > RT_SPACE_SIZE)
        PANIC("Arguments too large");

    {
        register word_t a2 __asm__ ("a2") = SYS_iss_set_argv;
        register word_t a3 __asm__ ("a3") = RT_SPACE_START + size;
        asm volatile ("simcall" : : "a"(a2), "a"(a3));

        e->argv = reinterpret_cast<char**>(RT_SPACE_START + size);
    }
}
#endif

void Env::pre_init() {
    // workaround to ensure that this gets linked in
    dummy[0] = &_Exception;

    // clear bss
    uintptr_t start = reinterpret_cast<uintptr_t>(&_bss_table_start);
    uintptr_t end = reinterpret_cast<uintptr_t>(&_bss_table_end);
    memset(&_bss_table_start, 0, end - start);

#if defined(__t3__)
    // started from simulator?
    if(entry == 0)
        get_args(this);
#endif
}

void Env::post_init() {
    // call constructors
    _init();
}

}
