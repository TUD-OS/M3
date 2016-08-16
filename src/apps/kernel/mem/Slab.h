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

#include <base/Common.h>
#include <base/col/SList.h>
#include <base/col/DList.h>
#include <base/util/Util.h>

namespace kernel {

class Slab : public m3::SListItem {
    struct Pool : public m3::DListItem {
        explicit Pool(size_t objsize, size_t count);
        ~Pool();

        size_t total;
        size_t free;
        void *mem;
    };

public:
#if defined(__t2__)
    static const size_t STEP_SIZE   = 8;
#else
    static const size_t STEP_SIZE   = 64;
#endif

    static Slab *get(size_t objsize);

    explicit Slab(size_t objsize) : _freelist(), _objsize(objsize) {
    }

    void *alloc();
    void free(void *ptr);

private:
    void **_freelist;
    size_t _objsize;
    m3::DList<Pool> _pools;
    static m3::SList<Slab> _slabs;
};

}
