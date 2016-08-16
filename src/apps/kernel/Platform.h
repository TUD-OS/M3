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

#include <base/PEDesc.h>

namespace kernel {

class Platform {
public:
    static const size_t MAX_MODS        = 64;
    static const size_t MAX_PES         = 64;

    struct KEnv {
        explicit KEnv();

        uintptr_t mods[MAX_MODS];
        size_t pe_count;
        m3::PEDesc pes[MAX_PES];
    } PACKED;

    static size_t kernel_pe();
    static size_t first_pe();
    static size_t last_pe();

    static uintptr_t mod(size_t i) {
        return _kenv.mods[i];
    }
    static size_t pe_count() {
        return _kenv.pe_count;
    }
    static m3::PEDesc pe(size_t no) {
        return _kenv.pes[no];
    }

    static uintptr_t def_recvbuf(size_t no);
    static uintptr_t rw_barrier(size_t no);

private:
    static KEnv _kenv;
};

}
