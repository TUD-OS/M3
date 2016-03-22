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

#include <m3/com/GateStream.h>
#include <m3/session/Session.h>
#include <m3/session/arch/host/Plasma.h>
#include <m3/session/arch/host/Keyboard.h>

using namespace m3;

static void kb_event(Plasma *plasma, RecvGate &gate, Subscriber<RecvGate&> *) {
    Keyboard::Event ev;
    GateIStream is(gate);
    is >> ev;
    if(ev.isbreak)
        return;
    switch(ev.keycode) {
        case Keyboard::VK_LEFT:
            plasma->left();
            break;
        case Keyboard::VK_RIGHT:
            plasma->right();
            break;
        case Keyboard::VK_UP:
            plasma->colup();
            break;
        case Keyboard::VK_DOWN:
            plasma->coldown();
            break;
    }
}

int main() {
    // create event gate
    Keyboard kb("keyb");
    Plasma plasma("plasma");
    kb.gate().subscribe(std::bind(kb_event, &plasma, std::placeholders::_1, std::placeholders::_2));

    env()->workloop()->run();
    return 0;
}
