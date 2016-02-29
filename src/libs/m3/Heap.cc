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
#include <m3/DTU.h>
#include <m3/Log.h>
#include <m3/stream/OStream.h>
#include <cstring>

#if defined(__t3__)
#   include <m3/arch/t3/RCTMux.h>
#endif

namespace m3 {

/*
 * The goal here is to reach a small code size, don't waste too much memory through internal or
 * external fragmentation and have a reasonably good allocate/free performance.
 * To achieve that, the heap puts the areas inside the memory it manages and constructs a double
 * linked list with it. But no pointers are used but the distance in bytes is stored. So, each
 * area begins with the struct Area, that contains the distance to the previous and next area,
 * followed by the data. If the area is used, i.e. not free, the MSB in the next field is set.
 * If there is no previous, prev is 0 and if there is is no next, a + a->next will point beyond
 * HEAP_END.
 */

bool Heap::_ready = false;
Heap::Area *Heap::_begin;
Heap::Area *Heap::_end;

void *Heap::alloc(size_t size) {
    void *res = try_alloc(size);
    if(!res)
        PANIC("Unable to alloc " << size << " bytes on the heap");

#if defined(__t3__)
    // update heap size if necessary
    // FIXME: needs optimization
    word_t *hsize = &(applayout()->data_size);
    *hsize = end() - applayout()->data_start;
#endif

    return res;
}

// TODO I haven't figured out yet why we need to flag some functions as used to ensure that they are
// including when using link-time-optimization and others not.
USED void *Heap::try_alloc(size_t size) {
    static_assert(ALIGN >= DTU_PKG_SIZE, "ALIGN is wrong");
    assert(size < USED_BIT);
    if(!_ready)
        init();

    // align it to at least word-size (the fortran-runtime seems to expect that). 8 is even better
    // because the DTU requires that.
    size = (size + sizeof(Area) + ALIGN - 1) & ~(ALIGN - 1);

    // find free area with enough space; start at the end, i.e. the large chunk of free memory
    Area *a = backwards(_end, _end->prev);
    do {
        if(!is_used(a) && a->next >= size)
            break;
        a = backwards(a, a->prev);
    }
    while(a->prev > 0);
    // for simplicity of error-handling we assume that every failed memory allocation is a
    // show stopper. thus, we terminate the program.
    if(is_used(a) || a->next < size)
        return nullptr;

    // is there space left? (take care that we need space for an area behind it and that it actually
    // makes sense to have this free, i.e. that it's >= the minimum size)
    if(a->next >= size + ALIGN) {
        // put a new area behind us
        Area *n = forward(a, size);
        n->next = a->next - size;
        n->prev = (n - a) * sizeof(Area);
        // adjust prev of next area, if there is any
        Area *nn = forward(n, n->next);
        nn->prev = (nn - n) * sizeof(Area);
        a->next = size;
    }
    // mark used
    a->next |= USED_BIT;
    return a + 1;
}

USED void *Heap::calloc(size_t n, size_t size) {
    void *ptr = alloc(n * size);
    memset(ptr, 0, n * size);
    return ptr;
}

USED void *Heap::realloc(void *p, size_t size) {
    if(!p)
        return alloc(size);

    /* allocate new area with requested size */
    void *newp = alloc(size);

    /* copy old content over and free old area */
    Area *a = backwards(reinterpret_cast<Area*>(p), sizeof(Area));
    memcpy(newp, p, (a->next & ~USED_BIT) - sizeof(Area));
    free(p);
    return newp;
}

USED void Heap::free(void *p) {
    if(p == nullptr)
        return;

    /* get area and the one behind */
    Area *a = backwards(reinterpret_cast<Area*>(p), sizeof(Area));
    a->next &= ~USED_BIT;
    Area *n = forward(a, a->next);

    if(a->prev) {
        Area *p = backwards(a, a->prev);
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
    if(n < _end && !is_used(n)) {
        Area *nn = forward(n, n->next);
        // so merge it
        a->next += n->next;
        // adjust prev of next area
        nn->prev = a->next;
    }

#if defined(__t3__)
    // update heap size if necessary
    // FIXME: optimize this
    word_t *size = &(applayout()->data_size);
    *size = end() - applayout()->data_start;
#endif
}

void Heap::print(OStream &os) {
    Area *a = _begin;
    os << "Heap[free=" << free_memory() << "]\n";
    while(a < _end) {
        os << "  " << (is_used(a) ? "u" : "-");
        os << " next=" << (a->next & ~USED_BIT);
        os << " prev=" << a->prev << "\n";
        a = forward(a, a->next & ~USED_BIT);
    }
}

size_t Heap::contiguous_mem() {
    size_t max = 0;
    if(!_ready)
        init();
    Area *a = _begin;
    while(a < _end) {
        if(!is_used(a) && a->next - sizeof(Area) > max)
            max = a->next - sizeof(Area);
        a = forward(a, a->next & ~USED_BIT);
    }
    return max;
}

size_t Heap::free_memory() {
    size_t total = 0;
    if(!_ready)
        init();
    Area *a = _begin;
    while(a < _end) {
        if(!is_used(a))
            total += a->next;
        a = forward(a, a->next & ~USED_BIT);
    }
    return total;
}

uintptr_t Heap::end() {
    if(!_ready)
        init();
    Area *last = nullptr;
    Area *a = _begin;
    while(a < _end) {
        if(is_used(a) && a > last)
            last = a;
        a = forward(a, a->next & ~USED_BIT);
    }
    if(last == nullptr)
        return reinterpret_cast<uintptr_t>(_begin + 1);
    return reinterpret_cast<uintptr_t>(forward(last, last->next & ~USED_BIT) + 1);
}

}
