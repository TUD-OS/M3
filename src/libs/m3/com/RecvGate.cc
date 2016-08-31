/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Init.h>

#include <m3/com/RecvGate.h>
#include <m3/com/SendGate.h>

namespace m3 {

INIT_PRIO_RECVGATE RecvGate RecvGate::_default (RecvGate::create(&RecvBuf::def()));

Errors::Code RecvGate::wait(SendGate *sgate) const {
    while(!DTU::get().fetch_msg(epid())) {
        if(sgate && !DTU::get().is_valid(sgate->epid()))
            return Errors::EP_INVALID;
        DTU::get().sleep();
    }
    return Errors::NO_ERROR;
}

}
