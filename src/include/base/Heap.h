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
#include <heap/heap.h>

namespace kernel {
class BaremetalKEnvBackend;
}

namespace m3 {

class OStream;

class Heap {
    friend class kernel::BaremetalKEnvBackend;

public:
    /**
     * Inits the heap
     */
    static void init();

    /**
     * Tries to allocate <size> bytes.
     *
     * @param size the number of bytes to allocate
     * @return the pointer to the allocated area or nullptr if there is not enough space.
     */
    static void *try_alloc(size_t size) {
        panic = false;
        void *res = alloc(size);
        panic = true;
        return res;
    }

    /**
     * Allocates <size> bytes.
     *
     * @param size the number of bytes to allocate
     * @return the pointer to the allocated area. does NOT return nullptr, if it fails.
     */
    static void *alloc(size_t size) {
        return heap_alloc(size);
    }

    /**
     * Allocates <n> * <size> bytes.
     *
     * @param n the number of elements
     * @param size the size of one element
     * @return the pointer to the allocated area. does NOT return nullptr, if it fails.
     */
    static void *calloc(size_t n, size_t size) {
        return heap_calloc(n, size);
    }

    /**
     * Tries to increase the area, pointed at by <p>, to <size> bytes. If this is not possible, it
     * will relocate the area to a place where <size> bytes are available.
     *
     * @param p the current area (might be nullptr)
     * @param size the new size of the area
     * @return the pointer to the allocated area. does NOT return nullptr, if it fails.
     */
    static void *realloc(void *p, size_t size) {
        return heap_realloc(p, size);
    }

    /**
     * Frees the given area
     *
     * @param p the pointer to the area (might be nullptr)
     */
    static void free(void *p) {
        return heap_free(p);
    }

    /**
     * Determines whether <p> was allocated on this heap.
     *
     * @param p the pointer
     * @return true if <p> is on this heap
     */
    static bool is_on_heap(const void *p) {
        const HeapArea *a = reinterpret_cast<const HeapArea*>(p);
        return a >= heap_begin && a < heap_end;
    }

    /**
     * Determines the maximum amount of contiguous memory that is currently available. You can use
     * this to allocate the as much memory as possible, for example. Keep in mind, though, that
     * some M3 abstractions allocate memory, too!
     *
     * @return the maximum amount of contiguous memory
     */
    static size_t contiguous_mem();

    /**
     * @return the total amount of free memory
     */
    static size_t free_memory() {
        return heap_free_memory();
    }

    /**
     * @return the end of the heap that is used.
     */
    static uintptr_t used_end() {
        return heap_used_end();
    }

    /**
     * @return the address of the end area
     */
    static uintptr_t end_area() {
        return reinterpret_cast<uintptr_t>(heap_end);
    }
    /**
     * @return the size of the end area
     */
    static size_t end_area_size() {
        return sizeof(HeapArea);
    }

    /**
     * Prints the areas of the heap
     *
     * @param os the OStream to print to
     */
    static void print(OStream &os);

private:
    static bool is_used(HeapArea *a) {
        return a->next & HEAP_USED_BITS;
    }
    static HeapArea *forward(HeapArea *a, size_t size) {
        return reinterpret_cast<HeapArea*>(reinterpret_cast<uintptr_t>(a) + size);
    }
    static HeapArea *backwards(HeapArea *a, size_t size) {
        return reinterpret_cast<HeapArea*>(reinterpret_cast<uintptr_t>(a) - size);
    }

    static void init_arch();

    static void alloc_callback(void *p, size_t size);
    static void free_callback(void *p);
    static bool oom_callback(size_t size);
    static void dblfree_callback(void *p);

    static bool panic;
};

}
