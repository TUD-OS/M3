/*
 * Copyright (C) 2015-2017, Nils Asmussen <nils@os.inf.tu-dresden.de>
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
#include <base/util/Math.h>
#include <base/Config.h>
#include <base/Heap.h>

#include <m3/VPE.h>

namespace m3 {

extern "C" void *_ResetVector_text_start;
extern "C" void *_ResetVector_text_end;
extern "C" void *_iram0_text_start;
extern "C" void *_text_end;
extern "C" void *_dram0_rodata_start;

uintptr_t VPE::get_entry() {
    return reinterpret_cast<uintptr_t>(&_ResetVector_text_start);
}

Errors::Code VPE::copy_sections() {
    /* copy reset vector */
    uintptr_t start_addr, end_addr;

    start_addr = Math::round_dn(reinterpret_cast<uintptr_t>(&_ResetVector_text_start), DTU_PKG_SIZE);
    end_addr = Math::round_up(reinterpret_cast<uintptr_t>(&_ResetVector_text_end), DTU_PKG_SIZE);
    _mem.write(reinterpret_cast<void*>(start_addr), end_addr - start_addr, start_addr);

    /* copy text */
    start_addr = Math::round_dn(reinterpret_cast<uintptr_t>(&_iram0_text_start), DTU_PKG_SIZE);
    end_addr = Math::round_up(reinterpret_cast<uintptr_t>(&_text_end), DTU_PKG_SIZE);
    _mem.write(reinterpret_cast<void*>(start_addr), end_addr - start_addr, start_addr);

    /* copy data and heap */
    start_addr = Math::round_dn(reinterpret_cast<uintptr_t>(&_dram0_rodata_start), DTU_PKG_SIZE);
    end_addr = Math::round_up(Heap::end(), DTU_PKG_SIZE);
    _mem.write(reinterpret_cast<void*>(start_addr), end_addr - start_addr, start_addr);

    /* copy end-area of heap */
    start_addr = Math::round_dn(reinterpret_cast<uintptr_t>(RT_START - DTU_PKG_SIZE), DTU_PKG_SIZE);
    _mem.write(reinterpret_cast<void*>(start_addr), DTU_PKG_SIZE, start_addr);

    /* copy stack */
    start_addr = get_sp();
    end_addr = STACK_TOP;
    _mem.write(reinterpret_cast<void*>(start_addr), end_addr - start_addr, start_addr);

    return Errors::NONE;
}

bool VPE::skip_section(ElfPh *ph) {
    /* 0x5b8 is the offset of .UserExceptionVector.literal. we exclude this because it
     * is used by idle for wakeup-interrupts. to now overwrite it, we simply don't
     * copy it over. if we're at it, we can also skip the handler-code itself and the overflow/
     * underflow handler. */
    return ph->p_vaddr == CODE_BASE_ADDR + 0x5b8 ||
           ph->p_vaddr == CODE_BASE_ADDR + 0x5bc ||
           ph->p_vaddr == CODE_BASE_ADDR + 0x400;
}

}
