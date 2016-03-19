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

#include <base/Common.h>
#include <base/arch/gem5/Exception.h>
#include <base/Env.h>

typedef void (*constr_func)();

void *__dso_handle;

extern constr_func CTORS_BEGIN;
extern constr_func CTORS_END;

namespace m3 {

void Env::pre_init() {
}

void Env::post_init() {
    m3::Exceptions::init();

    // call constructors
    for(constr_func *func = &CTORS_BEGIN; func < &CTORS_END; ++func)
        (*func)();
}

void Env::jmpto(uintptr_t addr) {
    if(addr != 0)
        asm volatile ("jmp *%0" : : "r"(addr));
    while(1)
        asm volatile ("hlt");
}

}
