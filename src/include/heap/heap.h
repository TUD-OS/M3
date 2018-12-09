/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#define HEAP_USED_BITS   (0x5UL << (sizeof(word_t) * 8 - 3))

typedef void (*heap_alloc_func)(void *p, size_t size);
typedef void (*heap_free_func)(void *p);
typedef bool (*heap_oom_func)(size_t size);
typedef void (*heap_dblfree_func)(void *p);

typedef struct HeapArea {
    word_t next;    /* HEAP_USED_BITS set = used */
    word_t prev;
    uint8_t _pad[64 - sizeof(word_t) * 2];
} PACKED HeapArea;

extern HeapArea *heap_begin;
extern HeapArea *heap_end;

EXTERN_C void heap_set_alloc_callback(heap_alloc_func callback);
EXTERN_C void heap_set_free_callback(heap_free_func callback);
EXTERN_C void heap_set_oom_callback(heap_oom_func callback);
EXTERN_C void heap_set_dblfree_callback(heap_dblfree_func callback);

EXTERN_C void *heap_alloc(size_t size);
EXTERN_C void *heap_calloc(size_t n, size_t size);
EXTERN_C void *heap_realloc(void *p, size_t size);
EXTERN_C void heap_free(void *p);

EXTERN_C void heap_append(size_t pages);

EXTERN_C size_t heap_free_memory();
EXTERN_C uintptr_t heap_used_end();
