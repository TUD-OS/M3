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
#include <base/stream/OStream.h>
#include <base/log/Lib.h>
#include <base/Heap.h>
#include <base/DTU.h>
#include <base/Panic.h>

namespace m3 {

bool Heap::panic = true;

void Heap::init() {
    if(m3::LibLog::level & m3::LibLog::HEAP) {
        heap_set_free_callback(free_callback);
        heap_set_alloc_callback(alloc_callback);
    }
    heap_set_oom_callback(oom_callback);
    heap_set_dblfree_callback(dblfree_callback);
    init_arch();
}

void Heap::print(OStream &os) {
    HeapArea *a = heap_begin;
    os << "Heap[free=" << free_memory() << "]\n";
    while(a < heap_end) {
        os << "  @ " << fmt((void*)a, "p") << " " << (is_used(a) ? "u" : "-");
        os << " next=" << (a->next & ~HEAP_USED_BITS);
        os << " prev=" << a->prev << "\n";
        a = forward(a, a->next & ~HEAP_USED_BITS);
    }
}

size_t Heap::contiguous_mem() {
    size_t max = 0;
    HeapArea *a = heap_begin;
    while(a < heap_end) {
        if(!is_used(a) && a->next - sizeof(HeapArea) > max)
            max = a->next - sizeof(HeapArea);
        a = forward(a, a->next & ~HEAP_USED_BITS);
    }
    return max;
}

size_t Heap::free_memory() {
    size_t total = 0;
    HeapArea *a = heap_begin;
    while(a < heap_end) {
        if(!is_used(a))
            total += a->next;
        a = forward(a, a->next & ~HEAP_USED_BITS);
    }
    return total;
}

void Heap::alloc_callback(void *p, size_t size) {
    if(Serial::ready()) {
        uintptr_t addr[6] = {0};
        Backtrace::collect(addr, ARRAY_SIZE(addr));
        LLOG(HEAP, "Allocated " << size << "b @ " << p << ":"
            << " " << fmt(addr[2], "0x")
            << " " << fmt(addr[3], "0x")
            << " " << fmt(addr[4], "0x")
            << " " << fmt(addr[5], "0x"));
    }
}

void Heap::free_callback(void *p) {
    if(Serial::ready()) {
        uintptr_t addr[6] = {0};
        Backtrace::collect(addr, ARRAY_SIZE(addr));
        LLOG(HEAP, "Freeing " << p << ":"
            << " " << fmt(addr[2], "0x")
            << " " << fmt(addr[3], "0x")
            << " " << fmt(addr[4], "0x")
            << " " << fmt(addr[5], "0x"));
    }
}

bool Heap::oom_callback(size_t size) {
    if(!env()->backend()->extend_heap(size)) {
        if(panic)
            PANIC("Unable to alloc " << size << " bytes on the heap (free=" << free_memory() << ")");
        return false;
    }
    return true;
}

void Heap::dblfree_callback(void *p) {
    PANIC("Used bits not set for " << p << "; double free?");
}

}
