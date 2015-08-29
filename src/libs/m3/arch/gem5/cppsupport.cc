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

#include <m3/Common.h>
#include <m3/cap/RecvGate.h>
#include <m3/Log.h>
#include <m3/Config.h>
#include <m3/ChanMng.h>
#include <m3/RecvBuf.h>
#include <m3/Machine.h>
#include <cstdlib>
#include <cstring>
#include <functional>

using namespace m3;

typedef void (*constr_func)();

namespace std {
void __throw_length_error(char const *s) {
    PANIC(s);
}

void __throw_bad_alloc() {
    PANIC("bad alloc");
}

void __throw_bad_function_call() {
    PANIC("bad function call");
}
}

void *operator new(size_t size) throw() {
    return Heap::alloc(size);
}

void *operator new[](size_t size) throw() {
    return Heap::alloc(size);
}

void operator delete(void *ptr) throw() {
    Heap::free(ptr);
}

void operator delete[](void *ptr) throw() {
    Heap::free(ptr);
}

EXTERN_C void __cxa_pure_virtual() {
    exit(1);
}

EXTERN_C void __cxa_atexit() {
    // TODO
}

namespace m3 {
    static RecvBuf *def_rbuf;
    RecvGate *def_rgate;
}

namespace std {
    namespace placeholders {
        const _Placeholder<1> _1{};
        const _Placeholder<2> _2{};
    }
}

void *__dso_handle;

extern constr_func CTORS_BEGIN;
extern constr_func CTORS_END;

EXTERN_C void __clibrary_init(int, char **argv) {
    bool kernel = argv && strstr(argv[0], "kernel") != nullptr;
    volatile CoreConf *cfg = coreconf();
    if(!kernel) {
        // wait until the kernel has initialized our core
        while(cfg->coreid == 0)
            ;
    }
    // the kernel is always on a predefined core
    else
        cfg->coreid = KERNEL_CORE;

    def_rbuf = new RecvBuf(RecvBuf::create(
        ChanMng::DEF_RECVCHAN, nextlog2<256>::val, nextlog2<128>::val, 0));
    def_rgate = new RecvGate(RecvGate::create(def_rbuf));

    // call constructors
    for(constr_func *func = &CTORS_BEGIN; func < &CTORS_END; ++func)
        (*func)();

    Serial::get().init(argv ? argv[0] : "Unknown", cfg->coreid);
}

EXTERN_C void __clibrary_init_lambda(int, char **argv) {
    volatile CoreConf *cfg = coreconf();
    // wait until the kernel has initialized our core
    while(cfg->coreid == 0)
        ;

    // setup default receive buffer
    // TODO actually, we need to do that for all receive buffers
    DTU::get().set_receiving(def_rbuf->chanid(), reinterpret_cast<word_t>(def_rbuf->addr()),
        def_rbuf->order(), def_rbuf->msgorder(), def_rbuf->flags());

    Serial::get().init(argv ? argv[0] : "Unknown", cfg->coreid);
}

EXTERN_C int lambda_main(std::function<int()> *f) {
    return (*f)();
}

/* Fortran support */
EXTERN_C void outbyte(char byte) {
    Machine::write(&byte, 1);
}

EXTERN_C uint8_t inbyte() {
    // TODO implement me
    return 0;
}

#ifndef NDEBUG
EXTERN_C void __assert(const char *failedexpr, const char *file, int line) throw() {
    Serial::get() << "assertion \"" << failedexpr << "\" failed in "
                  << file << ":" << line << "\n";
    abort();
    /* NOTREACHED */
}

EXTERN_C void __assert_fail(const char *failedexpr, const char *file, unsigned int line, const char *func) throw() {
    Serial::get() << "assertion \"" << failedexpr << "\" failed in " << func << " in "
                  << file << ":" << line << "\n";
    abort();
    /* NOTREACHED */
}
#endif
