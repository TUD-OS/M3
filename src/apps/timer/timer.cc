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

#include <m3/cap/Session.h>
#include <m3/service/arch/host/Interrupts.h>
#include <m3/Syscalls.h>
#include <m3/GateStream.h>
#include <m3/WorkLoop.h>
#include <m3/Log.h>

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

    WorkLoop::get().run();
    return 0;
}
