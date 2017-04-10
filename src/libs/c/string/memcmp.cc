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
#include <cstring>

int memcmp(const void *mem1, const void *mem2, size_t count) {
    const uint8_t *bmem1 = static_cast<const uint8_t*>(mem1);
    const uint8_t *bmem2 = static_cast<const uint8_t*>(mem2);
    for(size_t i = 0; i < count; i++) {
        if(bmem1[i] > bmem2[i])
            return 1;
        else if(bmem1[i] < bmem2[i])
            return -1;
    }
    return 0;
}
