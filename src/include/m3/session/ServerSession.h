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

#include <base/Errors.h>
#include <base/KIF.h>

#include <m3/server/Server.h>
#include <m3/ObjCap.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

namespace m3 {

/**
 * A server session is used to represent sessions at the server side.
 */
class ServerSession : public ObjCap {
public:
    /**
     * Creates a session for the given server.
     *
     * @param srv_sel the server selector
     * @param sel the desired selector
     */
    explicit ServerSession(capsel_t srv_sel, capsel_t _sel = ObjCap::INVALID)
        : ObjCap(SESSION) {
        if(srv_sel != ObjCap::INVALID) {
            if(_sel == ObjCap::INVALID)
                _sel = VPE::self().alloc_sel();
            Syscalls::get().createsess(_sel, srv_sel, reinterpret_cast<word_t>(this));
            sel(_sel);
        }
    }

    // has to be virtual, because we pass <this> as the ident to the kernel
    virtual ~ServerSession() {
    }
};

}
