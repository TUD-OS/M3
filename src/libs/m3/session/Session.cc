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

#include <m3/session/Session.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

namespace m3 {

void Session::connect(const String &service, word_t arg) {
    capsel_t cap = _vpe.alloc_cap();
    Errors::Code res = Syscalls::get().createsess(_vpe.sel(), cap, service, arg);
    if(res == Errors::NO_ERROR)
        sel(cap);
    else
        _vpe.free_cap(cap);
}

void Session::delegate(const KIF::CapRngDesc &crd, size_t *argcount, word_t *args) {
    Syscalls::get().delegate(_vpe.sel(), sel(), crd, argcount, args);
}

KIF::CapRngDesc Session::obtain(uint count, size_t *argcount, word_t *args) {
    KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, _vpe.alloc_caps(count), count);
    Syscalls::get().obtain(_vpe.sel(), sel(), crd, argcount, args);
    return crd;
}

}
