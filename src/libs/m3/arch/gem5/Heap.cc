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
#include <m3/Config.h>
#include <m3/Log.h>
#include <m3/Heap.h>

#include <sys/mman.h>

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
