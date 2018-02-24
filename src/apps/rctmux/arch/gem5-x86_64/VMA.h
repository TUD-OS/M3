/**
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
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

#include <base/DTU.h>

#include "Exceptions.h"

namespace RCTMux {

// virtual memory assistant
class VMA {
public:
    static void *dtu_irq(m3::Exceptions::State *state);
    static void *mmu_pf(m3::Exceptions::State *state);

private:
    static uintptr_t get_pte_addr(uintptr_t virt, int level);
    static m3::DTU::pte_t to_dtu_pte(uint64_t pte);
    static void resume_cmd();
    static bool handle_pf(m3::DTU::reg_t xlate_req, uintptr_t virt, uint perm);
    static bool handle_xlate(m3::DTU::reg_t xlate_req);
    static void *handle_ext_req(m3::Exceptions::State *state, m3::DTU::reg_t mst_req);
    static void terminate(m3::Exceptions::State *state, uintptr_t cr2);
};

}
