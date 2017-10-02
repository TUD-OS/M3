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
#include <base/log/Lib.h>
#include <base/DTU.h>
#include <heap/heap.h>
#include <string.h>

/*
 * The goal here is to reach a small code size, don't waste too much memory through internal or
 * external fragmentation and have a reasonably good allocate/free performance.
 * To achieve that, the heap puts the areas inside the memory it manages and constructs a double
 * linked list with it. But no pointers are used but the distance in bytes is stored. So, each
 * area begins with the struct HeapArea, that contains the distance to the previous and next area,
 * followed by the data. If the area is used, i.e. not free, the MSB in the next field is set.
 * If there is no previous, prev is 0 and if there is is no next, a + a->next will point beyond
 * HEAP_END.
 */

static const size_t ALIGN       = sizeof(HeapArea);

static heap_alloc_func alloc_callback;
static heap_free_func free_callback;
static heap_oom_func oom_callback;
static heap_dblfree_func dblfree_callback;

HeapArea *heap_begin;
HeapArea *heap_end;

static bool is_used(HeapArea *a) {
    return a->next & HEAP_USED_BITS;
}
static HeapArea *forward(HeapArea *a, size_t size) {
    return reinterpret_cast<HeapArea*>(reinterpret_cast<uintptr_t>(a) + size);
}
static HeapArea *backwards(HeapArea *a, size_t size) {
    return reinterpret_cast<HeapArea*>(reinterpret_cast<uintptr_t>(a) - size);
}

USED void heap_set_alloc_callback(heap_alloc_func callback) {
    alloc_callback = callback;
}
USED void heap_set_free_callback(heap_free_func callback) {
    free_callback = callback;
}
USED void heap_set_oom_callback(heap_oom_func callback) {
    oom_callback = callback;
}
USED void heap_set_dblfree_callback(heap_dblfree_func callback) {
    dblfree_callback = callback;
}

USED void *heap_alloc(size_t size) {
    static_assert(ALIGN >= DTU_PKG_SIZE, "ALIGN is wrong");
    // assert(size < HEAP_USED_BITS);

    // align it to at least word-size (the fortran-runtime seems to expect that). 8 is even better
    // because the DTU requires that.
    size = (size + sizeof(HeapArea) + ALIGN - 1) & ~(ALIGN - 1);

    // find free area with enough space; start at the end, i.e. the large chunk of free memory
    HeapArea *a = nullptr;
    while(1) {
        a = backwards(heap_end, heap_end->prev);
        do {
            if(!is_used(a) && a->next >= size)
                break;
            a = backwards(a, a->prev);
        }
        while(a->prev > 0);

        // have we found a suitable area?
        if(!is_used(a) && a->next >= size)
            break;

        // ok, try to extend the heap
        if(!oom_callback || !oom_callback(size))
            return nullptr;
    }

    // is there space left? (take care that we need space for an area behind it and that it actually
    // makes sense to have this free, i.e. that it's >= the minimum size)
    if(a->next >= size + ALIGN) {
        // put a new area behind us
        HeapArea *n = forward(a, size);
        n->next = a->next - size;
        n->prev = static_cast<word_t>(n - a) * sizeof(HeapArea);
        // adjust prev of next area, if there is any
        HeapArea *nn = forward(n, n->next);
        nn->prev = static_cast<word_t>(nn - n) * sizeof(HeapArea);
        a->next = size;
    }

    // mark used
    a->next |= HEAP_USED_BITS;

    if((m3::LibLog::level & m3::LibLog::HEAP) && alloc_callback)
        alloc_callback(a + 1, size);

    return a + 1;
}

USED void *heap_calloc(size_t n, size_t size) {
    void *ptr = heap_alloc(n * size);
    memset(ptr, 0, n * size);
    return ptr;
}

USED void *heap_realloc(void *p, size_t size) {
    if(!p)
        return heap_alloc(size);

    /* allocate new area with requested size */
    void *newp = heap_alloc(size);

    /* copy old content over and free old area */
    HeapArea *a = backwards(reinterpret_cast<HeapArea*>(p), sizeof(HeapArea));
    memcpy(newp, p, (a->next & ~HEAP_USED_BITS) - sizeof(HeapArea));
    heap_free(p);
    return newp;
}

USED void heap_free(void *p) {
    if(p == nullptr)
        return;

    if((m3::LibLog::level & m3::LibLog::HEAP) && free_callback)
        free_callback(p);

    /* get area and the one behind */
    HeapArea *a = backwards(reinterpret_cast<HeapArea*>(p), sizeof(HeapArea));
    if((a->next & HEAP_USED_BITS) != HEAP_USED_BITS) {
        if(dblfree_callback)
            dblfree_callback(p);
        return;
    }
    a->next &= ~HEAP_USED_BITS;
    HeapArea *n = forward(a, a->next);

    if(a->prev) {
        HeapArea *p = backwards(a, a->prev);
        // is prev already free? then merge it
        if(!is_used(p)) {
            p->next += a->next;
            // adjust prev of next area
            n->prev = p->next;
            // continue with the merged one
            a = p;
        }
    }

    // is there a next one and is it free?
    if(n < heap_end && !is_used(n)) {
        HeapArea *nn = forward(n, n->next);
        // so merge it
        a->next += n->next;
        // adjust prev of next area
        nn->prev = a->next;
    }
}
