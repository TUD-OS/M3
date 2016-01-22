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

#include <m3/Common.h>

namespace m3 {

class OStream;

/**
 * The heap implementation of M3. Optimized for a small code size and small memory overhead for
 * management, rather than performance.
 *
 * Note that all methods do NOT return nullptr in case there is not enough memory. The reasoning behind
 * that is, that since the heap has a fixed amount of memory and there is no multithreading, there
 * are no reasons why a failure should be handled. One could think of:
 * 1) trying to allocate memory and try again later if it failed.
 *    This does not make sense here since the available memory will never change.
 * 2) trying to allocate memory and try again with less if it failed.
 *    This is not required since one can use contiguous_mem() beforehand to find out if it would
 *    fail or how much one might allocate at most.
 * 3) report an error at the point of allocation for better error messages.
 *    This might be a valid usecase, but backtraces achieve basically the same (without overhead).
 *
 * Thus, for simplicity, the heap will terminate the program if allocation fails since this is
 * always considered as an unrecoverable error. This removes the need to check for the return value
 * at many places (which improves performance and decreases the code size).
 *
 * It is also possible to use the C interface (malloc, ...), but this class should be preferred.
 */
class Heap {
    struct Area {
        word_t next;    /* MSB set = used */
        word_t prev;
    } PACKED;

    static const word_t USED_BIT    = 1UL << (sizeof(word_t) * 8 - 1);
    static const size_t ALIGN       = sizeof(Area);

public:
    /**
     * Tries to allocate <size> bytes.
     *
     * @param size the number of bytes to allocate
     * @return the pointer to the allocated area or nullptr if there is not enough space.
     */
    static void *try_alloc(size_t size);

    /**
     * Allocates <size> bytes.
     *
     * @param size the number of bytes to allocate
     * @return the pointer to the allocated area. does NOT return nullptr, if it fails.
     */
    static void *alloc(size_t size);

    /**
     * Allocates <n> * <size> bytes.
     *
     * @param n the number of elements
     * @param size the size of one element
     * @return the pointer to the allocated area. does NOT return nullptr, if it fails.
     */
    static void *calloc(size_t n, size_t size);

    /**
     * Tries to increase the area, pointed at by <p>, to <size> bytes. If this is not possible, it
     * will relocate the area to a place where <size> bytes are available.
     *
     * @param p the current area (might be nullptr)
     * @param size the new size of the area
     * @return the pointer to the allocated area. does NOT return nullptr, if it fails.
     */
    static void *realloc(void *p, size_t size);

    /**
     * Frees the given area
     *
     * @param p the pointer to the area (might be nullptr)
     */
    static void free(void *p);

    /**
     * Determines whether <p> was allocated on this heap.
     *
     * @param p the pointer
     * @return true if <p> is on this heap
     */
    static bool is_on_heap(const void *p) {
        const Area *a = reinterpret_cast<const Area*>(p);
        return a >= _begin && a < _end;
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
    static size_t free_memory();

    /**
     * @return the end of the heap that is used.
     */
    static uintptr_t end();

    /**
     * Prints the areas of the heap
     *
     * @param os the OStream to print to
     */
    static void print(OStream &os);

private:
    static void init();
    static bool is_used(Area *a) {
        return a->next & USED_BIT;
    }
    static Area *forward(Area *a, size_t size) {
        return reinterpret_cast<Area*>(reinterpret_cast<uintptr_t>(a) + size);
    }
    static Area *backwards(Area *a, size_t size) {
        return reinterpret_cast<Area*>(reinterpret_cast<uintptr_t>(a) - size);
    }

    static bool _ready;
    static Area *_begin;
    static Area *_end;
};

}
