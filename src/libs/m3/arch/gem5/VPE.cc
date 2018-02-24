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

Errors::Code VPE::copy_sections() {
    goff_t start_addr, end_addr;

    if(_pager) {
        if(VPE::self().pager()) {
            _pager->clone();
            return Errors::NONE;
        }

        Errors::Code err;

        // map text
        start_addr = Math::round_dn(reinterpret_cast<uintptr_t>(&_text_start), DTU_PKG_SIZE);
        end_addr = Math::round_up(reinterpret_cast<uintptr_t>(&_text_end), DTU_PKG_SIZE);
        err = _pager->map_anon(&start_addr, end_addr - start_addr,
            Pager::READ | Pager::WRITE | Pager::EXEC, 0);
        if(err != Errors::NONE)
            return err;

        // map data
        start_addr = Math::round_dn(reinterpret_cast<uintptr_t>(&_data_start), DTU_PKG_SIZE);
        end_addr = Math::round_up(Heap::end_area() + Heap::end_area_size(), DTU_PKG_SIZE);
        err = _pager->map_anon(&start_addr, end_addr - start_addr, Pager::READ | Pager::WRITE, 0);
        if(err != Errors::NONE)
            return err;

        // map area for stack and boot/runtime stuff
        start_addr = RT_START;
        err = _pager->map_anon(&start_addr, STACK_TOP - start_addr, Pager::READ | Pager::WRITE, 0);
        if(err != Errors::NONE)
            return err;
    }

    /* copy text */
    start_addr = Math::round_dn(reinterpret_cast<uintptr_t>(&_text_start), DTU_PKG_SIZE);
    end_addr = Math::round_up(reinterpret_cast<uintptr_t>(&_text_end), DTU_PKG_SIZE);
    _mem.write(reinterpret_cast<void*>(start_addr), end_addr - start_addr, start_addr);

    /* copy data and heap */
    start_addr = Math::round_dn(reinterpret_cast<uintptr_t>(&_data_start), DTU_PKG_SIZE);
    end_addr = Math::round_up(Heap::used_end(), DTU_PKG_SIZE);
    _mem.write(reinterpret_cast<void*>(start_addr), end_addr - start_addr, start_addr);

    /* copy end-area of heap */
    start_addr = Heap::end_area();
    _mem.write(reinterpret_cast<void*>(start_addr), Heap::end_area_size(), start_addr);

    /* copy stack */
    start_addr = CPU::get_sp();
    end_addr = STACK_TOP;
    _mem.write(reinterpret_cast<void*>(start_addr), end_addr - start_addr, start_addr);

    return Errors::NONE;
}

bool VPE::skip_section(ElfPh *) {
    return false;
}

}
