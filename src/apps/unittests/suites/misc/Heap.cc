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
#include <m3/Config.h>
#include <m3/stream/Serial.h>
#include <cstdlib>

#include "Heap.h"

#define SINGLE_BYTE_COUNT 30

using namespace m3;

static uint *ptrs_single[SINGLE_BYTE_COUNT];

static size_t sizes[] = {1, 4, 10, 32, 67, 124, 56, 43};
static uint *ptrs[ARRAY_SIZE(sizes)];
static size_t rand_free1[] = {7, 5, 2, 0, 6, 3, 4, 1};
static size_t rand_free2[] = {3, 4, 1, 5, 6, 0, 7, 2};

static bool test_check_content(uint *ptr, size_t count, uint value) {
    while(count-- > 0) {
        if(*ptr++ != value)
            return false;
    }
    return true;
}

static void test_t1alloc() {
    for(size_t size = 0; size < ARRAY_SIZE(sizes); size++) {
        ptrs[size] = (uint*)Heap::alloc(sizes[size] * sizeof(uint));
        if(ptrs[size] == nullptr)
            Serial::get() << "Not enough mem\n";
        else {
            /* write test */
            *(ptrs[size]) = 4;
            *(ptrs[size] + sizes[size] - 1) = 2;
        }
    }
}

void HeapTestSuite::TestCase1::run() {
    check_heap_before();

    test_t1alloc();
    for(size_t size = 0; size < ARRAY_SIZE(sizes); size++) {
        if(ptrs[size] != nullptr) {
            Heap::free(ptrs[size]);
            /* write test */
            for(size_t i = size + 1; i < ARRAY_SIZE(sizes); i++) {
                if(ptrs[i] != nullptr) {
                    *(ptrs[i]) = 1;
                    *(ptrs[i] + sizes[i] - 1) = 2;
                }
            }
        }
    }

    check_heap_after();
}

void HeapTestSuite::TestCase2::run() {
    check_heap_before();

    test_t1alloc();
    for(ssize_t size = ARRAY_SIZE(sizes) - 1; size >= 0; size--) {
        if(ptrs[size] != nullptr) {
            Heap::free(ptrs[size]);
            /* write test */
            for(ssize_t i = size - 1; i >= 0; i--) {
                if(ptrs[i] != nullptr) {
                    *(ptrs[i]) = 1;
                    *(ptrs[i] + sizes[i] - 1) = 2;
                }
            }
        }
    }

    check_heap_after();
}

void HeapTestSuite::TestCase3::run() {
    check_heap_before();

    test_t1alloc();
    for(size_t size = 0; size < ARRAY_SIZE(sizes); size++) {
        if(ptrs[rand_free1[size]] != nullptr) {
            Heap::free(ptrs[rand_free1[size]]);
        }
    }

    check_heap_after();
}

void HeapTestSuite::TestCase4::run() {
    check_heap_before();

    test_t1alloc();
    for(size_t size = 0; size < ARRAY_SIZE(sizes); size++) {
        if(ptrs[rand_free2[size]] != nullptr) {
            Heap::free(ptrs[rand_free2[size]]);
        }
    }

    check_heap_after();
}

void HeapTestSuite::TestCase5::run() {
    for(size_t size = 0; size < ARRAY_SIZE(sizes); size++) {
        Serial::get() << "Allocate and free " << sizes[size] * sizeof(uint)<< " bytes\n";
        check_heap_before();

        ptrs[0] = (uint*)Heap::alloc(sizes[size] * sizeof(uint));
        if(ptrs[0] != nullptr) {
            /* write test */
            *(ptrs[0]) = 1;
            *(ptrs[0] + sizes[size] - 1) = 2;
            Heap::free(ptrs[0]);

            check_heap_after();
        }
        else
            Serial::get() << "Not enough mem\n";
    }
}

void HeapTestSuite::TestCase6::run() {
    check_heap_before();

    for(size_t i = 0; i < SINGLE_BYTE_COUNT; i++) {
        ptrs_single[i] = (uint*)Heap::alloc(1);
    }
    for(size_t i = 0; i < SINGLE_BYTE_COUNT; i++) {
        Heap::free(ptrs_single[i]);
    }

    check_heap_after();
}

void HeapTestSuite::TestCase7::run() {
    uint *ptr1, *ptr2, *ptr3, *ptr4, *ptr5;
    check_heap_before();

    ptr1 = (uint*)Heap::alloc(4 * sizeof(uint));
    for(size_t i = 0; i < 4; i++)
        *(ptr1 + i) = 1;
    ptr2 = (uint*)Heap::alloc(8 * sizeof(uint));
    for(size_t i = 0; i < 8; i++)
        *(ptr2 + i) = 2;
    ptr3 = (uint*)Heap::alloc(12 * sizeof(uint));
    for(size_t i = 0; i < 12; i++)
        *(ptr3 + i) = 3;
    Heap::free(ptr2);
    ptr4 = (uint*)Heap::alloc(6 * sizeof(uint));
    for(size_t i = 0; i < 6; i++)
        *(ptr4 + i) = 4;
    ptr5 = (uint*)Heap::alloc(2 * sizeof(uint));
    for(size_t i = 0; i < 2; i++)
        *(ptr5 + i) = 5;

    assert_true(test_check_content(ptr1, 4, 1));
    assert_true(test_check_content(ptr3, 12, 3));
    assert_true(test_check_content(ptr4, 6, 4));
    assert_true(test_check_content(ptr5, 2, 5));

    Heap::free(ptr1);
    Heap::free(ptr3);
    Heap::free(ptr4);
    Heap::free(ptr5);

    check_heap_after();
}

void HeapTestSuite::TestCase8::run() {
    size_t i;
    uint *p, *ptr1, *ptr2, *ptr3;

    check_heap_before();

    ptr1 = (uint*)Heap::alloc(10 * sizeof(uint));
    for(p = ptr1, i = 0; i < 10; i++)
        *p++ = 1;

    ptr2 = (uint*)Heap::alloc(5 * sizeof(uint));
    for(p = ptr2, i = 0; i < 5; i++)
        *p++ = 2;

    ptr3 = (uint*)Heap::alloc(2 * sizeof(uint));
    for(p = ptr3, i = 0; i < 2; i++)
        *p++ = 3;

    ptr2 = (uint*)Heap::realloc(ptr2, 10 * sizeof(uint));

    /* check content */
    assert_true(test_check_content(ptr1, 10, 1));
    assert_true(test_check_content(ptr3, 2, 3));
    assert_true(test_check_content(ptr2, 5, 2));

    /* fill 2 completely */
    for(p = ptr2, i = 0; i < 10; i++)
        *p++ = 2;

    ptr3 = (uint*)Heap::realloc(ptr3, 6 * sizeof(uint));

    /* check content */
    assert_true(test_check_content(ptr1, 10, 1));
    assert_true(test_check_content(ptr2, 10, 2));
    assert_true(test_check_content(ptr3, 2, 3));

    /* fill 3 completely */
    for(p = ptr3, i = 0; i < 6; i++)
        *p++ = 3;

    ptr3 = (uint*)Heap::realloc(ptr3, 7 * sizeof(uint));

    /* check content */
    assert_true(test_check_content(ptr1, 10, 1));
    assert_true(test_check_content(ptr2, 10, 2));
    assert_true(test_check_content(ptr3, 6, 3));

    /* fill 3 completely */
    for(p = ptr3, i = 0; i < 7; i++)
        *p++ = 3;

    /* free all */
    Heap::free(ptr1);
    Heap::free(ptr2);
    Heap::free(ptr3);

    check_heap_after();
}

void HeapTestSuite::TestCase9::run() {
    check_heap_before();

    void *ptrs[HEAP_SIZE / 0x200];

    // free backwards
    ssize_t i;
    for(i = 0; i < (ssize_t)ARRAY_SIZE(ptrs); ++i) {
        ptrs[i] = Heap::try_alloc(0x200);
        if(ptrs[i] == nullptr)
            break;
    }
    assert_true(ptrs[i] == nullptr);
    for(; i >= 0; --i)
        Heap::free(ptrs[i]);

    // free forward
    for(i = 0; i < (ssize_t)ARRAY_SIZE(ptrs); ++i) {
        ptrs[i] = Heap::try_alloc(0x200);
        if(ptrs[i] == nullptr)
            break;
    }
    for(ssize_t j = 0; j < i; ++j)
        Heap::free(ptrs[j]);

    check_heap_after();
}
