/**
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/Syscalls.h>
#include <m3/VPE.h>

struct RemoteServer {
    explicit RemoteServer(m3::VPE &vpe, const m3::String &name)
        : srv(m3::ObjCap::SERVICE, m3::VPE::self().alloc_sels(2)),
          rgate(m3::RecvGate::create_for(vpe, srv.sel() + 1, m3::nextlog2<256>::val,
                                                             m3::nextlog2<256>::val)) {
        rgate.activate();
        m3::Syscalls::get().createsrv(srv.sel(), vpe.sel(), rgate.sel(), name);
        vpe.delegate(m3::KIF::CapRngDesc(m3::KIF::CapRngDesc::OBJ, srv.sel(), 2));
    }

    void request_shutdown() {
        m3::Syscalls::get().srvctrl(srv.sel(), m3::KIF::Syscall::SCTRL_SHUTDOWN);
    }

    m3::String sel_arg() const {
        m3::OStringStream os;
        os << srv.sel() << " " << rgate.ep();
        return os.str();
    }

    m3::ObjCap srv;
    m3::RecvGate rgate;
};
