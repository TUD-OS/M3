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

#include <base/arch/host/HWInterrupts.h>

#include <m3/com/Gate.h>
#include <m3/com/RecvGate.h>
#include <m3/session/Session.h>
#include <m3/VPE.h>

namespace m3 {

class Interrupts : public Session {
public:
    explicit Interrupts(const String &service, HWInterrupts::IRQ irq,
                        int buford = nextlog2<256>::val,
                        int msgord = nextlog2<64>::val)
        : Session(service, irq),
          _rgate(RecvGate::create(buford, msgord)),
          _sgate(SendGate::create(&_rgate, 0, SendGate::UNLIMITED)) {
        if(!Errors::occurred())
            delegate_obj(_sgate.sel());
    }

    RecvGate &rgate() {
        return _rgate;
    }

private:
    RecvGate _rgate;
    SendGate _sgate;
};

}
