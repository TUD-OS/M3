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
#include <m3/cap/VPE.h>
#include <m3/util/Math.h>
#include <m3/Config.h>
#include <m3/Heap.h>

namespace m3 {

extern "C" void *_ResetVector_text_start;
extern "C" void *_ResetVector_text_end;
extern "C" void *_iram0_text_start;
extern "C" void *_text_end;
extern "C" void *_dram0_rodata_start;

word_t VPE::get_sp() {
    word_t val;
    asm volatile (
          "mov.n %0, a1;"
          : "=a" (val)
    );
    return val;
}

uintptr_t VPE::get_entry() {
    return (uintptr_t)&_ResetVector_text_start;
}

void VPE::copy_sections() {
    /* copy reset vector */
    uintptr_t start_addr = Math::round_dn((uintptr_t)&_ResetVector_text_start, DTU_PKG_SIZE);
    uintptr_t end_addr = Math::round_up((uintptr_t)&_ResetVector_text_end, DTU_PKG_SIZE);
    _mem.write_sync((void*)start_addr, end_addr - start_addr, start_addr);

    /* copy text */
    start_addr = Math::round_dn((uintptr_t)&_iram0_text_start, DTU_PKG_SIZE);
    end_addr = Math::round_up((uintptr_t)&_text_end, DTU_PKG_SIZE);
    _mem.write_sync((void*)start_addr, end_addr - start_addr, start_addr);

    /* copy data and heap */
    start_addr = Math::round_dn((uintptr_t)&_dram0_rodata_start, DTU_PKG_SIZE);
    end_addr = Math::round_up(Heap::end(), DTU_PKG_SIZE);
    _mem.write_sync((void*)start_addr, end_addr - start_addr, start_addr);

    /* copy end-area of heap */
    start_addr = Math::round_dn((uintptr_t)(RT_START - DTU_PKG_SIZE), DTU_PKG_SIZE);
    _mem.write_sync((void*)start_addr, DTU_PKG_SIZE, start_addr);

    /* copy stack */
    start_addr = get_sp();
    end_addr = STACK_TOP;
    _mem.write_sync((void*)start_addr, end_addr - start_addr, start_addr);
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
