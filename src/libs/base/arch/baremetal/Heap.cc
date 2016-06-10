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
#include <base/util/Math.h>
#include <base/Config.h>
#include <base/Heap.h>
#include <base/Env.h>

extern void *_bss_end;

namespace m3 {

void Heap::init() {
    uintptr_t begin = reinterpret_cast<uintptr_t>(&_bss_end);
    _begin = reinterpret_cast<Area*>(Math::round_up<size_t>(begin, sizeof(Area)));

    uintptr_t end;
    if(env()->heapsize == 0) {
#if defined(__gem5__)
        if(env()->pe.has_memory())
            end = env()->pe.mem_size() - RECVBUF_SIZE_SPM;
        // this does only exist so that we can still run scenarios on cache-PEs without pager
        else
            end = Math::round_up(begin, PAGE_SIZE) + MOD_HEAP_SIZE;
#else
        end = Math::round_dn<size_t>(RT_START, sizeof(Area));
#endif
    }
    else
        end = Math::round_up<size_t>(begin, PAGE_SIZE) + env()->heapsize;
    _end = reinterpret_cast<Area*>(end) - 1;

    _end->next = 0;
    _end->prev = (_end - _begin) * sizeof(Area);
    Area *a = _begin;
    a->next = (_end - _begin) * sizeof(Area);
    a->prev = 0;
    _ready = true;
}

}
