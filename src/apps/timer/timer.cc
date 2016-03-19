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

#include <base/Log.h>

#include <m3/com/GateStream.h>
#include <m3/session/Session.h>
#include <m3/session/arch/host/Interrupts.h>
#include <m3/Syscalls.h>

using namespace m3;

static void timer_event(RecvGate &gate, Subscriber<RecvGate&> *) {
    static int i = 0;
    GateIStream is(gate);
    HWInterrupts::IRQ irq;
    is >> irq;
    LOG(DEF, "Got IRQ #" << irq << " (" << i++ << ")");
}

int main() {
    Interrupts timerirqs("interrupts", HWInterrupts::TIMER);
    if(Errors::occurred())
        PANIC("Unable to connect to service 'interrupts'");
    timerirqs.gate().subscribe(timer_event);

    env()->workloop()->run();
    return 0;
}
