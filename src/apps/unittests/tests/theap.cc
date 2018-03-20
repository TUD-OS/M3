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
#include <base/Config.h>

#include <m3/stream/Standard.h>

#include <cstdlib>

#include "../unittests.h"

#define SINGLE_BYTE_COUNT 30

using namespace m3;

static uint *ptrs_single[SINGLE_BYTE_COUNT];

static size_t sizes[] = {1, 4, 10, 32, 67, 124, 56, 43};
static uint *ptrs[ARRAY_SIZE(sizes)];
static size_t rand_free1[] = {7, 5, 2, 0, 6, 3, 4, 1};
static size_t rand_free2[] = {3, 4, 1, 5, 6, 0, 7, 2};

static size_t heap_before;

static bool test_check_content(uint *ptr, size_t count, uint value) {
    while(count-- > 0) {
        if(*ptr++ != value)
            return false;
    }
    return true;
}

static void test_t1alloc() {
    for(size_t size = 0; size < ARRAY_SIZE(sizes); size++) {
        ptrs[size] = static_cast<uint*>(Heap::alloc(sizes[size] * sizeof(uint)));
        if(ptrs[size] == nullptr)
            cout << "Not enough mem\n";
        else {
            /* write test */
            *(ptrs[size]) = 4;
            *(ptrs[size] + sizes[size] - 1) = 2;
        }
    }
}

static void check_heap_before() {
    heap_before = m3::Heap::free_memory();
}

static void check_heap_after() {
    size_t after = m3::Heap::free_memory();
    assert_size(after, heap_before);
}

static void alloc_then_free_in_same_direction() {
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

static void allocate_then_free_in_opposite_direction() {
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

static void allocate_then_free_in_random_direction_1() {
    check_heap_before();

    test_t1alloc();
    for(size_t size = 0; size < ARRAY_SIZE(sizes); size++) {
        if(ptrs[rand_free1[size]] != nullptr) {
            Heap::free(ptrs[rand_free1[size]]);
        }
    }

    check_heap_after();
}

static void allocate_then_free_in_random_direction_2() {
    check_heap_before();

    test_t1alloc();
    for(size_t size = 0; size < ARRAY_SIZE(sizes); size++) {
        if(ptrs[rand_free2[size]] != nullptr) {
            Heap::free(ptrs[rand_free2[size]]);
        }
    }

    check_heap_after();
}

static void allocate_and_free_with_different_sizes() {
    for(size_t size = 0; size < ARRAY_SIZE(sizes); size++) {
        cout << "Allocate and free " << sizes[size] * sizeof(uint)<< " bytes\n";
        check_heap_before();

        ptrs[0] = static_cast<uint*>(Heap::alloc(sizes[size] * sizeof(uint)));
        if(ptrs[0] != nullptr) {
            /* write test */
            *(ptrs[0]) = 1;
            *(ptrs[0] + sizes[size] - 1) = 2;
            Heap::free(ptrs[0]);

            check_heap_after();
        }
        else
            cout << "Not enough mem\n";
    }
}

static void allocate_single_bytes() {
    check_heap_before();

    for(size_t i = 0; i < SINGLE_BYTE_COUNT; i++) {
        ptrs_single[i] = static_cast<uint*>(Heap::alloc(1));
    }
    for(size_t i = 0; i < SINGLE_BYTE_COUNT; i++) {
        Heap::free(ptrs_single[i]);
    }

    check_heap_after();
}

static void allocate_3_region() {
    uint *ptr1, *ptr2, *ptr3, *ptr4, *ptr5;
    check_heap_before();

    ptr1 = static_cast<uint*>(Heap::alloc(4 * sizeof(uint)));
    for(size_t i = 0; i < 4; i++)
        *(ptr1 + i) = 1;
    ptr2 = static_cast<uint*>(Heap::alloc(8 * sizeof(uint)));
    for(size_t i = 0; i < 8; i++)
        *(ptr2 + i) = 2;
    ptr3 = static_cast<uint*>(Heap::alloc(12 * sizeof(uint)));
    for(size_t i = 0; i < 12; i++)
        *(ptr3 + i) = 3;
    Heap::free(ptr2);
    ptr4 = static_cast<uint*>(Heap::alloc(6 * sizeof(uint)));
    for(size_t i = 0; i < 6; i++)
        *(ptr4 + i) = 4;
    ptr5 = static_cast<uint*>(Heap::alloc(2 * sizeof(uint)));
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

static void reallocate() {
    size_t i;
    uint *p, *ptr1, *ptr2, *ptr3;

    check_heap_before();

    ptr1 = static_cast<uint*>(Heap::alloc(10 * sizeof(uint)));
    for(p = ptr1, i = 0; i < 10; i++)
        *p++ = 1;

    ptr2 = static_cast<uint*>(Heap::alloc(5 * sizeof(uint)));
    for(p = ptr2, i = 0; i < 5; i++)
        *p++ = 2;

    ptr3 = static_cast<uint*>(Heap::alloc(2 * sizeof(uint)));
    for(p = ptr3, i = 0; i < 2; i++)
        *p++ = 3;

    ptr2 = static_cast<uint*>(Heap::realloc(ptr2, 10 * sizeof(uint)));

    /* check content */
    assert_true(test_check_content(ptr1, 10, 1));
    assert_true(test_check_content(ptr3, 2, 3));
    assert_true(test_check_content(ptr2, 5, 2));

    /* fill 2 completely */
    for(p = ptr2, i = 0; i < 10; i++)
        *p++ = 2;

    ptr3 = static_cast<uint*>(Heap::realloc(ptr3, 6 * sizeof(uint)));

    /* check content */
    assert_true(test_check_content(ptr1, 10, 1));
    assert_true(test_check_content(ptr2, 10, 2));
    assert_true(test_check_content(ptr3, 2, 3));

    /* fill 3 completely */
    for(p = ptr3, i = 0; i < 6; i++)
        *p++ = 3;

    ptr3 = static_cast<uint*>(Heap::realloc(ptr3, 7 * sizeof(uint)));

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

static void allocate_all_and_free_it_again() {
    check_heap_before();

    if(VPE::self().pe().has_cache()) {
        cout << "Skipping alloc-all test case on cache PE.\n";
        return;
    }

    void *ptrs[HEAP_SIZE / 0x4000];

    // free backwards
    ssize_t i;
    for(i = 0; i < static_cast<ssize_t>(ARRAY_SIZE(ptrs)); ++i) {
        ptrs[i] = Heap::try_alloc(0x4000);
        if(ptrs[i] == nullptr)
            break;
    }
    assert_true(ptrs[i] == nullptr);
    for(i--; i >= 0; --i)
        Heap::free(ptrs[i]);

    // free forward
    for(i = 0; i < static_cast<ssize_t>(ARRAY_SIZE(ptrs)); ++i) {
        ptrs[i] = Heap::try_alloc(0x4000);
        if(ptrs[i] == nullptr)
            break;
    }
    for(ssize_t j = 0; j < i; ++j)
        Heap::free(ptrs[j]);

    check_heap_after();
}

void theap() {
    RUN_TEST(alloc_then_free_in_same_direction);
    RUN_TEST(allocate_then_free_in_opposite_direction);
    RUN_TEST(allocate_then_free_in_random_direction_1);
    RUN_TEST(allocate_then_free_in_random_direction_2);
    RUN_TEST(allocate_and_free_with_different_sizes);
    RUN_TEST(allocate_single_bytes);
    RUN_TEST(allocate_3_region);
    RUN_TEST(reallocate);
    RUN_TEST(allocate_all_and_free_it_again);
}
