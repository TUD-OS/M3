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

#pragma once

#include "mem/Slab.h"

namespace kernel {

class SlabCache {
public:
    explicit SlabCache(size_t objsize) : _slab(Slab::get(objsize)) {
    }

    void *alloc() {
        return _slab->alloc();
    }
    void free(void *ptr) {
        _slab->free(ptr);
    }

private:
    Slab *_slab;
};

template<class T>
class SlabObject {
public:
    static void *operator new(size_t) {
        return _cache.alloc();
    }
    static void operator delete(void *ptr) {
        _cache.free(ptr);
    }

private:
    static SlabCache _cache;
};

template<typename T>
SlabCache SlabObject<T>::_cache(sizeof(T));

}
