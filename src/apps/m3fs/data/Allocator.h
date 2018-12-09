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

#include <fs/internal.h>

#include "../sess/Request.h"

class FSHandle;

class Allocator {
public:
    explicit Allocator(const char *name, uint32_t first, uint32_t *first_free, uint32_t *free,
                       uint32_t total, uint32_t blocks);

    uint32_t alloc(Request &r) {
        size_t count = 1;
        return alloc(r, &count);
    }
    uint32_t alloc(Request &r, size_t *count);
    void free(Request &r, uint32_t start, size_t count);

private:
    const char *_name;
    uint32_t _first;
    uint32_t *_first_free;
    uint32_t *_free;
    uint32_t _total;
    uint32_t _blocks;
};
