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
#include <base/Config.h>

#include "mem/AddrSpace.h"
#include "mem/MainMemory.h"
#include "DTU.h"

namespace kernel {

AddrSpace::AddrSpace(vpeid_t vpeid, epid_t ep, capsel_t gate)
    : _vpeid(vpeid),
      _ep(ep),
      _gate(gate),
      _root() {
    MainMemory::Allocation rootpt = MainMemory::get().allocate(PAGE_SIZE, PAGE_SIZE);
    _root = m3::DTU::build_gaddr(rootpt.pe(), rootpt.addr);
}

AddrSpace::~AddrSpace() {
    // the root PT is free'd via the recursive entry
    DTU::get().remove_pts(_vpeid);
}

}
