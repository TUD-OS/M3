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
#include <base/DTU.h>

#include "mem/AddrSpace.h"
#include "mem/MainMemory.h"

namespace kernel {

AddrSpace::AddrSpace(epid_t ep, capsel_t gate)
    : _ep(ep),
      _gate(gate),
      _rootpt(MainMemory::get().allocate(PAGE_SIZE)) {
}

AddrSpace::~AddrSpace() {
    MainMemory::get().free(_rootpt);
}

}
