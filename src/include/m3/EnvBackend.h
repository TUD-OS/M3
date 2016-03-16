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

#include <m3/Common.h>

namespace m3 {

class RecvGate;
class RecvBuf;

class EnvBackend {
public:
    explicit EnvBackend() {
    }
    virtual ~EnvBackend() {
    }

    virtual void attach_recvbuf(RecvBuf *rb);
    virtual void detach_recvbuf(RecvBuf *rb);

    virtual void switch_ep(size_t victim, capsel_t oldcap, capsel_t newcap);

    RecvGate *def_recvgate;
};

}
