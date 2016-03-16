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

#include <m3/EnvBackend.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>

namespace m3 {

void EnvBackend::switch_ep(size_t victim, capsel_t oldcap, capsel_t newcap) {
    if(Syscalls::get().activate(victim, oldcap, newcap) != Errors::NO_ERROR) {
        // if we wanted to deactivate a cap, we can ignore the failure
        if(newcap != ObjCap::INVALID)
            PANIC("Unable to arm SEP " << victim << ": " << Errors::last);
    }
}

}
