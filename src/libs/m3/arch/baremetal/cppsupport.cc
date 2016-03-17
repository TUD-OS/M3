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
#include <m3/Heap.h>
#include <m3/Machine.h>
#include <m3/Log.h>
#include <functional>

using namespace m3;

namespace std {
    namespace placeholders {
        // two are enough for our purposes
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
    PANIC("pure virtual function call");
}

EXTERN_C void __cxa_atexit() {
    // TODO
}

/* Fortran support */
EXTERN_C void outbyte(char byte) {
    Machine::write(&byte, 1);
}

EXTERN_C uint8_t inbyte() {
    // TODO implement me
    return 0;
}
