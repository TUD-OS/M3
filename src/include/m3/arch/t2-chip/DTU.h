/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

namespace m3 {

inline void DTU::set_target(int, uchar dst, uintptr_t addr, uint, uchar) {
    volatile uint *ptr = reinterpret_cast<uint*>(PE_DMA_CONFIG);
    ptr[PE_DMA_REG_TARGET]      = dst;
    ptr[PE_DMA_REG_REM_ADDR]    = addr;
}

inline void DTU::fire(int, Operation op, const void *msg, size_t size) {
    volatile uint *ptr = reinterpret_cast<uint*>(PE_DMA_CONFIG);
    // currently we have to substract the DRAM start
    UNUSED uintptr_t addr = reinterpret_cast<uintptr_t>(msg);
    // both have to be packet-size aligned
    assert((addr & (PACKET_SIZE - 1)) == 0);
    assert((size & (PACKET_SIZE - 1)) == 0);

    ptr[PE_DMA_REG_TYPE]        = op == READ ? 0 : 2;
    ptr[PE_DMA_REG_LOC_ADDR]    = addr;
    ptr[PE_DMA_REG_SIZE]        = size;
}

inline size_t DTU::get_remaining(int) {
    volatile uint *ptr = reinterpret_cast<uint*>(PE_DMA_CONFIG);
    return ptr[PE_DMA_REG_SIZE];
}

}
