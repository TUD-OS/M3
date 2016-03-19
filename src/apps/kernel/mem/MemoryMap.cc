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

#include <base/Log.h>

#include <assert.h>

#include "mem/MemoryMap.h"

namespace kernel {

MemoryMap::MemoryMap(uintptr_t addr, size_t size) : list(new Area()) {
    list->addr = addr;
    list->size = size;
    list->next = nullptr;
}

MemoryMap::~MemoryMap() {
    for(Area *a = list; a != nullptr;) {
        Area *n = a->next;
        delete a;
        a = n;
    }
    list = nullptr;
}

uintptr_t MemoryMap::allocate(size_t size) {
    Area *a;
    Area *p = nullptr;
    for(a = list; a != nullptr; p = a, a = a->next) {
        if(a->size >= size)
            break;
    }
    if(a == nullptr)
        return -1;

    /* take it from the front */
    uintptr_t res = a->addr;
    a->size -= size;
    a->addr += size;
    /* if the area is empty now, remove it */
    if(a->size == 0) {
        if(p)
            p->next = a->next;
        else
            list = a->next;
        delete a;
    }
    LOG(MEM, "Requested " << (size / 1024) << " KiB of memory @ " << m3::fmt(res, "p"));
    return res;
}

void MemoryMap::free(uintptr_t addr, size_t size) {
    LOG(MEM, "Free'd " << (size / 1024) << " KiB of memory @ " << m3::fmt(addr, "p"));

    /* find the area behind ours */
    Area *n, *p = nullptr;
    for(n = list; n != nullptr && addr > n->addr; p = n, n = n->next)
        ;

    /* merge with prev and next */
    if(p && p->addr + p->size == addr && n && addr + size == n->addr) {
        p->size += size + n->size;
        p->next = n->next;
        delete n;
    }
    /* merge with prev */
    else if(p && p->addr + p->size == addr) {
        p->size += size;
    }
    /* merge with next */
    else if(n && addr + size == n->addr) {
        n->addr -= size;
        n->size += size;
    }
    /* create new area between them */
    else {
        Area *a = new Area();
        a->addr = addr;
        a->size = size;
        if(p)
            p->next = a;
        else
            list = a;
        a->next = n;
    }
}

size_t MemoryMap::get_size(size_t *areas) const {
    size_t total = 0;
    *areas = 0;
    for(Area *a = list; a != nullptr; a = a->next) {
        total += a->size;
        (*areas)++;
    }
    return total;
}

}
