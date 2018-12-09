/*
 * Copyright (C) 2015-2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/session/arch/host/Keyboard.h>

class Scancodes {
    struct Entry {
        unsigned char def;
        unsigned char ext;
    };

public:
    static bool get_keycode(bool &isbreak, unsigned char &keycode, unsigned char scancode);

private:
    static bool is_ext;
    static bool is_break_flag;
    static Entry table[];
};
