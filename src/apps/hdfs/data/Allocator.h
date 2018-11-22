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

#include "../MetaBuffer.h"

class FSHandle;

class Allocator {
public:
    explicit Allocator(uint32_t first, uint32_t *first_free, uint32_t *free,
                       uint32_t total, uint32_t blocks);

    uint32_t alloc(FSHandle &h) {
        size_t count = 1;
        return alloc(h, &count);
    }
    uint32_t alloc(FSHandle &h, size_t *count);
    void free(FSHandle &h, uint32_t start, size_t count);

private:
    uint32_t _first;
    uint32_t *_first_free;
    uint32_t *_free;
    uint32_t _total;
    uint32_t _blocks;
};
