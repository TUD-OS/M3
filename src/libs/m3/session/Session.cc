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

void Session::connect(const String &service, xfer_t arg, capsel_t cap) {
    if(cap == ObjCap::INVALID)
        cap = VPE::self().alloc_cap();
    Errors::Code res = Syscalls::get().createsess(cap, service, arg);
    if(res == Errors::NONE)
        sel(cap);
}

void Session::delegate_for(VPE &vpe, const KIF::CapRngDesc &crd, KIF::ExchangeArgs *args) {
    Syscalls::get().delegate(vpe.sel(), sel(), crd, args);
}

void Session::obtain_for(VPE &vpe, const KIF::CapRngDesc &crd, KIF::ExchangeArgs *args) {
    vpe.mark_caps_allocated(crd.start(), crd.count());
    Syscalls::get().obtain(vpe.sel(), sel(), crd, args);
}

}
