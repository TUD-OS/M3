/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#pragma once

#include <m3/cap/Session.h>
#include <m3/cap/SendGate.h>
#include <m3/cap/MemGate.h>
#include <m3/cap/VPE.h>

namespace m3 {

class Memory : public Session {
public:
    enum Operation {
        PAGEFAULT,
        MAP,
        UNMAP,
        COUNT,
    };

    enum Prot {
        READ    = MemGate::R,
        WRITE   = MemGate::W,
        EXEC    = MemGate::X,
    };

    explicit Memory(const String &service, const VPE &vpe)
        : Session(service), _gate(SendGate::bind(obtain(1).start())) {
        delegate(CapRngDesc(CapRngDesc::OBJ, vpe.sel(), 1));
    }

    const SendGate &gate() const {
        return _gate;
    }

    Errors::Code map(uintptr_t *virt, size_t len, int prot, int flags);
    Errors::Code unmap(uintptr_t virt);

private:
    SendGate _gate;
};

}
