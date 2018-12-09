/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Heap.h>
#include <base/log/Kernel.h>

#include "mem/Slab.h"

namespace kernel {

m3::SList<Slab> Slab::_slabs;

Slab::Pool::Pool(size_t objsize, size_t count)
    : total(count),
      free(count),
      mem(m3::Heap::alloc(objsize * count)) {
}

Slab::Pool::~Pool() {
    m3::Heap::free(mem);
}

Slab *Slab::get(size_t objsize) {
    assert(objsize >= sizeof(word_t));

    size_t size = 1UL << m3::getnextlog2(objsize + sizeof(word_t));
    for(auto s = _slabs.begin(); s != _slabs.end(); ++s) {
        if(s->_objsize == size) {
            KLOG(SLAB, "Using " << s->_objsize << "B slab for " << objsize << "B objects");
            return &*s;
        }
    }

    KLOG(SLAB, "Creating " << size << "B slab for " << objsize << "B objects");
    Slab *s = new Slab(size);
    _slabs.append(s);
    return s;
}

void *Slab::alloc() {
    if(EXPECT_FALSE(!_freelist)) {
        KLOG(SLAB, "Extending " << _objsize << "B slab by " << (_objsize * STEP_SIZE) << "B");

        Pool *p = new Pool(_objsize, STEP_SIZE);
        void **mem = reinterpret_cast<void**>(p->mem);
        void **end = mem + (_objsize * STEP_SIZE) / sizeof(void*);
        while(mem < end) {
            mem[0] = p;
            mem[1] = _freelist;
            _freelist = mem;
            mem += _objsize / sizeof(void*);
        }
        _pools.append(p);
    }

    void **ptr = _freelist;
    reinterpret_cast<Pool*>(ptr[0])->free--;
    _freelist = reinterpret_cast<void**>(_freelist[1]);
    return ptr + 1;
}

void Slab::free(void *addr) {
    void **ptr = reinterpret_cast<void**>(addr) - 1;

    Pool *p = reinterpret_cast<Pool*>(ptr[0]);
    p->free++;

    // the object should be somewhere in its pool
    assert(ptr >= p->mem && ptr < (void**)p->mem + (_objsize * STEP_SIZE) / sizeof(void*));
    assert(p->free <= p->total);

    // don't free pools for now. otherwise we shrink and extend back and forth.
    // TODO maybe we should do that only on memory pressure or so
#if 0
    if(EXPECT_FALSE(p->free == p->total)) {
        KLOG(SLAB, "Shrinking " << _objsize << "B slab by " << (p->total * _objsize) << "B");

        // remove all objects in the pool from the freelist
        void **obj = _freelist, **prev = nullptr;
        while(obj != nullptr) {
            if(obj[0] == p) {
                if(prev)
                    prev[1] = obj[1];
                else
                    _freelist = reinterpret_cast<void**>(obj[1]);
            }
            else
                prev = obj;
            obj = reinterpret_cast<void**>(obj[1]);
        }

        _pools.remove(p);
        delete p;
    }
    else
#endif
    {
        ptr[1] = _freelist;
        _freelist = ptr;
    }
}

}
