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

void Heap::init_arch() {
    uintptr_t begin = reinterpret_cast<uintptr_t>(&_bss_end);
    heap_begin = reinterpret_cast<HeapArea*>(Math::round_up<size_t>(begin, sizeof(HeapArea)));

    uintptr_t end;
    if(env()->heapsize == 0) {
#if defined(__gem5__)
        if(env()->pedesc.has_memory())
            end = env()->pedesc.mem_size() - RECVBUF_SIZE_SPM;
        // this does only exist so that we can still run scenarios on cache-PEs without pager
        else
            end = Math::round_up(begin, PAGE_SIZE) + MOD_HEAP_SIZE;
#else
        end = Math::round_dn<size_t>(RT_START, sizeof(HeapArea));
#endif
    }
    // TODO temporary
    else if(env()->pedesc.has_mmu() && env()->pe == 0)
        end = Math::round_up<size_t>(begin, PAGE_SIZE) + 1024 * 1024;
    else
        end = Math::round_up<size_t>(begin, PAGE_SIZE) + env()->heapsize;
    heap_end = reinterpret_cast<HeapArea*>(end) - 1;

    heap_end->next = 0;
    heap_end->prev = static_cast<size_t>(heap_end - heap_begin) * sizeof(HeapArea);
    HeapArea *a = heap_begin;
    a->next = static_cast<size_t>(heap_end - heap_begin) * sizeof(HeapArea);
    a->prev = 0;
}

}
