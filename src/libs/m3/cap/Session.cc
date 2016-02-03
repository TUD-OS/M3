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

#include <m3/cap/Session.h>
#include <m3/cap/VPE.h>
#include <m3/Syscalls.h>
#include <m3/Log.h>

namespace m3 {

void Session::connect(const String &service, const GateOStream &args) {
    capsel_t cap = _vpe.alloc_cap();
    Errors::Code res = Syscalls::get().createsess(_vpe.sel(), cap, service, args);
    if(res == Errors::NO_ERROR)
        sel(cap);
    else
        _vpe.free_cap(cap);
}

void Session::delegate(const CapRngDesc &crd) {
    Syscalls::get().delegate(_vpe.sel(), sel(), crd);
}

GateIStream Session::delegate(const CapRngDesc &crd, const GateOStream &args) {
    GateIStream reply = Syscalls::get().delegate(_vpe.sel(), sel(), crd, args);
    reply >> Errors::last;
    return reply;
}

CapRngDesc Session::obtain(uint count) {
    CapRngDesc crd(CapRngDesc::OBJ, _vpe.alloc_caps(count), count);
    Syscalls::get().obtain(_vpe.sel(), sel(), crd);
    return crd;
}

GateIStream Session::obtain(uint count, CapRngDesc &crd, const GateOStream &args) {
    crd = CapRngDesc(CapRngDesc::OBJ, _vpe.alloc_caps(count), count);
    GateIStream reply = Syscalls::get().obtain(_vpe.sel(), sel(), crd, args);
    reply >> Errors::last;
    return reply;
}

}
