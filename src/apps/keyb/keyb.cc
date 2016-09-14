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

#include <base/arch/host/SharedMemory.h>
#include <base/util/Sync.h>
#include <base/log/Services.h>

#include <m3/com/GateStream.h>
#include <m3/session/arch/host/Keyboard.h>
#include <m3/session/arch/host/Interrupts.h>
#include <m3/session/Session.h>
#include <m3/server/Server.h>
#include <m3/server/EventHandler.h>
#include <m3/Syscalls.h>

#include <unistd.h>
#include <fcntl.h>

#include "Scancodes.h"

#define KEYBOARD_CTRL       0       /* keyboard control register */
#define KEYBOARD_DATA       1       /* keyboard data register */

#define KEYBOARD_RDY        0x01    /* keyboard has a character */

using namespace m3;

static void kb_irq(Server<EventHandler<>> *kbserver, GateIStream &, Subscriber<GateIStream&> *) {
    static SharedMemory kbdmem("kbd", sizeof(unsigned) * 2, SharedMemory::JOIN);
    unsigned *regs = reinterpret_cast<unsigned*>(kbdmem.addr());
    if(regs[KEYBOARD_CTRL] & KEYBOARD_RDY) {
        unsigned data = regs[KEYBOARD_DATA];
        Sync::compiler_barrier();
        regs[KEYBOARD_CTRL] &= ~KEYBOARD_RDY;

        Keyboard::Event ev;
        ev.scancode = data;
        if(Scancodes::get_keycode(ev.isbreak, ev.keycode, ev.scancode)) {
            SLOG(KEYB, "Got " << (unsigned)ev.keycode << ":" << (unsigned)ev.isbreak);
            static_cast<EventHandler<>&>(kbserver->handler()).broadcast(ev);
        }
    }
}

int main() {
    Interrupts kbirqs("interrupts", HWInterrupts::KEYB);
    Server<EventHandler<>> kbserver("keyb", new EventHandler<>());
    kbirqs.gate().subscribe(std::bind(kb_irq, &kbserver, std::placeholders::_1, std::placeholders::_2));

    env()->workloop()->run();
    return 0;
}
