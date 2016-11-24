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
#include <base/util/Math.h>
#include <base/Config.h>
#include <base/Heap.h>

#include <m3/session/Pager.h>
#include <m3/VPE.h>

namespace m3 {

extern "C" void *_text_start;
extern "C" void *_text_end;
extern "C" void *_data_start;
extern "C" void *_bss_end;

uintptr_t VPE::get_entry() {
    return reinterpret_cast<uintptr_t>(&_text_start);
}

void VPE::copy_sections() {
    if(_pager) {
        _pager->clone();
        return;
    }

    uintptr_t start_addr, end_addr;

    /* copy text */
    start_addr = Math::round_dn(reinterpret_cast<uintptr_t>(&_text_start), DTU_PKG_SIZE);
    end_addr = Math::round_up(reinterpret_cast<uintptr_t>(&_text_end), DTU_PKG_SIZE);
    _mem.write(reinterpret_cast<void*>(start_addr), end_addr - start_addr, start_addr);

    /* copy data and heap */
    start_addr = Math::round_dn(reinterpret_cast<uintptr_t>(&_data_start), DTU_PKG_SIZE);
    end_addr = Math::round_up(Heap::end(), DTU_PKG_SIZE);
    _mem.write(reinterpret_cast<void*>(start_addr), end_addr - start_addr, start_addr);

    /* copy end-area of heap */
    start_addr = Heap::end_area();
    _mem.write(reinterpret_cast<void*>(start_addr), Heap::end_area_size(), start_addr);

    /* copy stack */
    start_addr = CPU::get_sp();
    end_addr = STACK_TOP;
    _mem.write(reinterpret_cast<void*>(start_addr), end_addr - start_addr, start_addr);
}

bool VPE::skip_section(ElfPh *) {
    return false;
}

}
