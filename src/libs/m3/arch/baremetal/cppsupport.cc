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
#include <m3/cap/VPE.h>
#include <m3/tracing/Tracing.h>
#include <m3/Log.h>
#include <m3/Config.h>
#include <m3/RecvBuf.h>
#include <m3/Machine.h>
#include <cstdlib>
#include <cstring>
#include <functional>

using namespace m3;

EXTERN_C void _init();

namespace m3 {
    static RecvBuf *def_rbuf;
    RecvGate *def_rgate;
}

#if defined(__t2__) or defined(__t3__)
EXTERN_C void *_Exception;
static void *dummy[1];
#endif

namespace std {
    namespace placeholders {
        const _Placeholder<1> _1{};
        const _Placeholder<2> _2{};
    }
}

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

EXTERN_C void __clibrary_init(int, char **argv) {
#if defined(__t2__) or defined(__t3__)
    // workaround to ensure that this gets linked in
    dummy[0] = &_Exception;
#endif

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

    def_rbuf = new RecvBuf(RecvBuf::bindto(
        DTU::DEF_RECVEP, reinterpret_cast<void*>(DEF_RCVBUF), DEF_RCVBUF_ORDER, 0));
    def_rgate = new RecvGate(RecvGate::create(def_rbuf));

    Serial::init(argv ? argv[0] : "Unknown", cfg->coreid);

    // call constructors
    _init();
}

EXTERN_C void __clibrary_init_lambda(int, char **argv) {
    volatile CoreConf *cfg = coreconf();
    // wait until the kernel has initialized our core
    while(cfg->coreid == 0)
        ;

#if defined(__t3__)
    // set default receive buffer again
    DTU::get().configure_recv(def_rbuf->epid(), reinterpret_cast<word_t>(def_rbuf->addr()),
        def_rbuf->order(), def_rbuf->msgorder(), def_rbuf->flags());
#endif

    Serial::init(argv ? argv[0] : "Unknown", cfg->coreid);
    EPMux::get().reset();
#if defined(__t2__)
    DTU::get().reset();
#endif

    VPE::self().init_state();

    EVENT_TRACE_REINIT();
}

EXTERN_C int lambda_main(std::function<int()> *f) {
    EVENT_TRACER_lambda_main();
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
