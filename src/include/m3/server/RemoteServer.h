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
#include <base/stream/OStringStream.h>

#include <m3/Syscalls.h>
#include <m3/VPE.h>

namespace m3 {

struct RemoteServer {
    explicit RemoteServer(VPE &vpe, const String &name)
        : srv(ObjCap::SERVICE, VPE::self().alloc_sels(2)),
          rgate(RecvGate::create_for(vpe, srv.sel() + 1, nextlog2<256>::val,
                                                             nextlog2<256>::val)) {
        rgate.activate();
        Syscalls::get().createsrv(srv.sel(), vpe.sel(), rgate.sel(), name);
        vpe.delegate(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, srv.sel(), 2));
    }

    void request_shutdown() {
        Syscalls::get().srvctrl(srv.sel(), KIF::Syscall::SCTRL_SHUTDOWN);
    }

    String sel_arg() const {
        OStringStream os;
        os << srv.sel() << " " << rgate.ep();
        return os.str();
    }

    ObjCap srv;
    RecvGate rgate;
};

}
