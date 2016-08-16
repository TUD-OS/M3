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

#include <m3/session/Session.h>
#include <m3/com/SendGate.h>
#include <m3/com/GateStream.h>

namespace m3 {

class Plasma : public Session {
public:
    enum Operation {
        LEFT,
        RIGHT,
        COLUP,
        COLDOWN,
        COUNT
    };

    explicit Plasma(const String &service)
        : Session(service), _gate(SendGate::bind(obtain(1).start())) {
    }

    int left() {
        return execute(LEFT);
    }
    int right() {
        return execute(RIGHT);
    }
    int colup() {
        return execute(COLUP);
    }
    int coldown() {
        return execute(COLDOWN);
    }

private:
    int execute(Operation op) {
        int res;
        GateIStream reply = send_receive_vmsg(_gate, op);
        reply >> res;
        return res;
    }

    SendGate _gate;
};

}
