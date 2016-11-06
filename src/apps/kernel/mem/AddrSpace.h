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

#include <base/Common.h>
#include <base/DTU.h>

#include "mem/MainMemory.h"

namespace kernel {

class AddrSpace {
public:
    explicit AddrSpace(epid_t ep, capsel_t gate);
    ~AddrSpace();

    epid_t ep() const {
        return _ep;
    }
    capsel_t gate() const {
        return _gate;
    }
    uint64_t root_pt() const {
        return m3::DTU::build_noc_addr(_rootpt.pe(), _rootpt.addr);
    }

    epid_t _ep;
    capsel_t _gate;
    MainMemory::Allocation _rootpt;
};

}
