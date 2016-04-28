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

#include <m3/session/Session.h>
#include <m3/session/arch/host/Interrupts.h>
#include <m3/stream/Standard.h>

using namespace m3;

static void timer_event(GateIStream &is, Subscriber<GateIStream&> *) {
    static int i = 0;
    HWInterrupts::IRQ irq;
    is >> irq;
    cout << "Got IRQ #" << irq << " (" << i++ << ")\n";
}

int main() {
    Interrupts timerirqs("interrupts", HWInterrupts::TIMER);
    if(Errors::occurred())
        exitmsg("Unable to connect to service 'interrupts'");
    timerirqs.gate().subscribe(timer_event);

    env()->workloop()->run();
    return 0;
}
