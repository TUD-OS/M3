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
#include <m3/com/GateStream.h>
#include <m3/VPE.h>

namespace m3 {

class Keyboard : public Session {
public:
    struct Event {
        unsigned char scancode;
        unsigned char keycode;
        bool isbreak;
    };

    enum Keycodes {
        VK_ACCENT,
        VK_0,
        VK_1,
        VK_2,
        VK_3,
        VK_4,
        VK_5,
        VK_6,
        VK_7,
        VK_8,
        VK_9,
        VK_MINUS,
        VK_EQ,
        VK_BACKSP,
        VK_TAB,
        VK_Q,
        VK_W,
        VK_E,
        VK_R,
        VK_T,
        VK_Y,
        VK_U,
        VK_I,
        VK_O,
        VK_P,
        VK_LBRACKET,
        VK_RBRACKET,
        VK_BACKSLASH,
        VK_CAPS,
        VK_A,
        VK_S,
        VK_D,
        VK_F,
        VK_G,
        VK_H,
        VK_J,
        VK_K,
        VK_L,
        VK_SEM,
        VK_APOS,
        /* non-US-1 ?? */
        VK_ENTER,
        VK_LSHIFT,
        VK_Z,
        VK_X,
        VK_C,
        VK_V,
        VK_B,
        VK_N,
        VK_M,
        VK_COMMA,
        VK_DOT,
        VK_SLASH,
        VK_RSHIFT,
        VK_LCTRL,
        VK_LSUPER,
        VK_LALT,
        VK_SPACE,
        VK_RALT,
        VK_APPS,  /* ?? */
        VK_RCTRL,
        VK_RSUPER,
        VK_INSERT,
        VK_DELETE,
        VK_HOME,
        VK_END,
        VK_PGUP,
        VK_PGDOWN,
        VK_LEFT,
        VK_UP,
        VK_DOWN,
        VK_RIGHT,
        VK_NUM,
        VK_KP7,
        VK_KP4,
        VK_KP1,
        VK_KPDIV,
        VK_KP8,
        VK_KP5,
        VK_KP2,
        VK_KP0,
        VK_KPMUL,
        VK_KP9,
        VK_KP6,
        VK_KP3,
        VK_KPDOT,
        VK_KPSUB,
        VK_KPADD,
        VK_KPENTER,
        VK_ESC,
        VK_F1,
        VK_F2,
        VK_F3,
        VK_F4,
        VK_F5,
        VK_F6,
        VK_F7,
        VK_F8,
        VK_F9,
        VK_F10,
        VK_F11,
        VK_F12,
        VK_PRINT,
        VK_SCROLL,
        VK_PAUSE,
        VK_PIPE
    };

    explicit Keyboard(const String &service, int buford = nextlog2<256>::val, int msgord = nextlog2<64>::val)
        : Session(service), _rgate(RecvGate::create(buford, msgord)),
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
