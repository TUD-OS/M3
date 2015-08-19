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

inline void DTU::set_target(int slot, uchar dst, uintptr_t addr, uint credits, uchar perm) {
    assert(slot >= 4);
    assert((credits & ~0xFF) == 0);
    UNUSED const uint dstslot = 0;
    volatile uint *ptr = reinterpret_cast<uint*>(PE_IDMA_CONFIG_ADDRESS +
        ((slot) << PE_IDMA_SLOT_POS) + 0x10 + ((dstslot) << 3));
    ptr[0] = addr;
    ptr[1] = (credits << 24) | (perm << 16) | dst;
}

inline void DTU::fire(int slot, Operation op, const void *msg, size_t size) {
    assert(slot >= 4);
    volatile uint *ptr = reinterpret_cast<uint*>(PE_IDMA_CONFIG_ADDRESS + (slot << PE_IDMA_SLOT_POS));
    // currently we have to substract the DRAM start
    UNUSED uintptr_t addr = (reinterpret_cast<uintptr_t>(msg) & 0xFFFFFF) - DRAM_START;
    // check limits
    assert((addr & ~0xFFFFFF) == 0);
    assert((size & ~0xFFFF) == 0);
    // both have to be packet-size aligned
    assert((addr & (PACKET_SIZE - 1)) == 0);
    assert((size & (PACKET_SIZE - 1)) == 0);

    // size is specified in packets of 8 bytes
    size /= PACKET_SIZE;
    ptr[0] = (size << 24) | addr;
    // cta=0, fire=1, op, stride=0, repeat=0, size
    ptr[1] = (0 << 30) | (1 << 29) | (op << 28) | (0 << 16) | (0 << 16) | ((size >> 8) & 0xFF);
}

inline size_t DTU::get_remaining(int) {
    return 0;
}

}
