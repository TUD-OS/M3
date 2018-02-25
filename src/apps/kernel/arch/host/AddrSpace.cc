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

#include "mem/AddrSpace.h"

namespace kernel {

void AddrSpace::setup(const VPEDesc &) {
    // not supported
}

void AddrSpace::set_rootpt_remote(const VPEDesc &) {
    // not supported
}

void AddrSpace::map_pages(const VPEDesc &, goff_t, gaddr_t, uint, int) {
    // not supported
}

void AddrSpace::unmap_pages(const VPEDesc &, goff_t, uint) {
    // not supported
}

void AddrSpace::remove_pts(vpeid_t) {
    // not supported
}

}
