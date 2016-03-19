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
#include <base/Config.h>
#include <base/Log.h>
#include <base/Heap.h>

#include <sys/mman.h>
#include <malloc.h>

/* these functions are defined as weak, so that we can simply overwrite them here */
USED void *malloc(size_t size) {
    return m3::Heap::alloc(size);
}
USED void *calloc(size_t n, size_t size) {
    return m3::Heap::calloc(n, size);
}
USED void *realloc(void *p, size_t size) {
    return m3::Heap::realloc(p, size);
}
USED void free(void *p) {
    return m3::Heap::free(p);
}

namespace m3 {

void Heap::init() {
    _begin = reinterpret_cast<Area*>(mmap(0, HEAP_SIZE, PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    if(_begin == MAP_FAILED)
        PANIC("Unable to map heap");

    _end = _begin + (HEAP_SIZE / sizeof(Area)) - sizeof(Area);
    _end->next = 0;
    _end->prev = (_end - _begin) * sizeof(Area);
    Area *a = _begin;
    a->next = (_end - _begin) * sizeof(Area);
    a->prev = 0;
    _ready = true;
}

}
