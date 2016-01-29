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

#include <m3/cap/Gate.h>
#include <m3/Syscalls.h>
#include <m3/Errors.h>

namespace m3 {

Errors::Code Gate::async_cmd(Operation op, void *data, size_t datalen, size_t off, size_t size,
        label_t reply_lbl, int reply_ep) {
    // ensure that the DMAUnit is ready. this is required if we want to mix async sends with
    // sync sends.
    wait_until_sent();
    ensure_activated();

retry:
    Errors::Code res;
    switch(op) {
        case SEND:
            res = DTU::get().send(_epid, data, datalen, reply_lbl, reply_ep);
            break;
        case READ:
            res = DTU::get().read(_epid, data, datalen, off);
            break;
        case WRITE:
            res = DTU::get().write(_epid, data, datalen, off);
            break;
        case CMPXCHG:
            res = DTU::get().cmpxchg(_epid, data, datalen, off, size);
            break;
    }
    if(res == Errors::VPE_GONE) {
        res = Syscalls::get().activate(_epid, sel(), sel());
        if(res != Errors::NO_ERROR)
            return res;
        goto retry;
    }
    return res;
}

}
