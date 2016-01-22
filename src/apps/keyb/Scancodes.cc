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

#include "Scancodes.h"

using namespace m3;

bool Scancodes::is_ext = 0;
bool Scancodes::is_break_flag = false;
Scancodes::Entry Scancodes::table[] = {
    /* 00 */    {0,                             0},
    /* 01 */    {Keyboard::VK_F9,               0},
    /* 02 */    {0,                             0},
    /* 03 */    {Keyboard::VK_F5,               0},
    /* 04 */    {Keyboard::VK_F3,               0},
    /* 05 */    {Keyboard::VK_F1,               0},
    /* 06 */    {Keyboard::VK_F2,               0},
    /* 07 */    {Keyboard::VK_F12,              0},
    /* 08 */    {0,                             0},
    /* 09 */    {Keyboard::VK_F10,              0},
    /* 0A */    {Keyboard::VK_F8,               0},
    /* 0B */    {Keyboard::VK_F6,               0},
    /* 0C */    {Keyboard::VK_F4,               0},
    /* 0D */    {Keyboard::VK_TAB,              0},
    /* 0E */    {Keyboard::VK_ACCENT,           0},
    /* 0F */    {0,                             0},
    /* 10 */    {0,                             0},
    /* 11 */    {Keyboard::VK_LALT,             Keyboard::VK_RALT},
    /* 12 */    {Keyboard::VK_LSHIFT,           0},
    /* 13 */    {0,                             0},
    /* 14 */    {Keyboard::VK_LCTRL,            Keyboard::VK_RCTRL},
    /* 15 */    {Keyboard::VK_Q,                0},
    /* 16 */    {Keyboard::VK_1,                0},
    /* 17 */    {0,                             0},
    /* 18 */    {0,                             0},
    /* 19 */    {0,                             0},
    /* 1A */    {Keyboard::VK_Z,                0},
    /* 1B */    {Keyboard::VK_S,                0},
    /* 1C */    {Keyboard::VK_A,                0},
    /* 1D */    {Keyboard::VK_W,                0},
    /* 1E */    {Keyboard::VK_2,                0},
    /* 1F */    {0,                             Keyboard::VK_LSUPER},
    /* 20 */    {0,                             0},
    /* 21 */    {Keyboard::VK_C,                0},
    /* 22 */    {Keyboard::VK_X,                0},
    /* 23 */    {Keyboard::VK_D,                0},
    /* 24 */    {Keyboard::VK_E,                0},
    /* 25 */    {Keyboard::VK_4,                0},
    /* 26 */    {Keyboard::VK_3,                0},
    /* 27 */    {0,                             Keyboard::VK_RSUPER},
    /* 28 */    {0,                             0},
    /* 29 */    {Keyboard::VK_SPACE,            0},
    /* 2A */    {Keyboard::VK_V,                0},
    /* 2B */    {Keyboard::VK_F,                0},
    /* 2C */    {Keyboard::VK_T,                0},
    /* 2D */    {Keyboard::VK_R,                0},
    /* 2E */    {Keyboard::VK_5,                0},
    /* 2F */    {0,                             Keyboard::VK_APPS},
    /* 30 */    {0,                             0},
    /* 31 */    {Keyboard::VK_N,                0},
    /* 32 */    {Keyboard::VK_B,                0},
    /* 33 */    {Keyboard::VK_H,                0},
    /* 34 */    {Keyboard::VK_G,                0},
    /* 35 */    {Keyboard::VK_Y,                0},
    /* 36 */    {Keyboard::VK_6,                0},
    /* 37 */    {0,                             0},
    /* 38 */    {0,                             0},
    /* 39 */    {0,                             0},
    /* 3A */    {Keyboard::VK_M,                0},
    /* 3B */    {Keyboard::VK_J,                0},
    /* 3C */    {Keyboard::VK_U,                0},
    /* 3D */    {Keyboard::VK_7,                0},
    /* 3E */    {Keyboard::VK_8,                0},
    /* 3F */    {0,                             0},
    /* 40 */    {0,                             0},
    /* 41 */    {Keyboard::VK_COMMA,            0},
    /* 42 */    {Keyboard::VK_K,                0},
    /* 43 */    {Keyboard::VK_I,                0},
    /* 44 */    {Keyboard::VK_O,                0},
    /* 45 */    {Keyboard::VK_0,                0},
    /* 46 */    {Keyboard::VK_9,                0},
    /* 47 */    {0,                             0},
    /* 48 */    {0,                             0},
    /* 49 */    {Keyboard::VK_DOT,              0},
    /* 4A */    {Keyboard::VK_SLASH,            Keyboard::VK_KPDIV},
    /* 4B */    {Keyboard::VK_L,                0},
    /* 4C */    {Keyboard::VK_SEM,              0},
    /* 4D */    {Keyboard::VK_P,                0},
    /* 4E */    {Keyboard::VK_MINUS,            0},
    /* 4F */    {0,                             0},
    /* 50 */    {0,                             0},
    /* 51 */    {0,                             0},
    /* 52 */    {Keyboard::VK_APOS,             0},
    /* 53 */    {0,                             0},
    /* 54 */    {Keyboard::VK_LBRACKET,         0},
    /* 55 */    {Keyboard::VK_EQ,               0},
    /* 56 */    {0,                             0},
    /* 57 */    {0,                             0},
    /* 58 */    {Keyboard::VK_CAPS,             0},
    /* 59 */    {Keyboard::VK_RSHIFT,           0},
    /* 5A */    {Keyboard::VK_ENTER,            Keyboard::VK_KPENTER},
    /* 5B */    {Keyboard::VK_RBRACKET,         0},
    /* 5C */    {0,                             0},
    /* 5D */    {Keyboard::VK_BACKSLASH,        0},
    /* 5E */    {0,                             0},
    /* 5F */    {0,                             0},
    /* 60 */    {0,                             0},
    /* 61 */    {Keyboard::VK_PIPE,             0},
    /* 62 */    {0,                             0},
    /* 63 */    {0,                             0},
    /* 64 */    {0,                             0},
    /* 65 */    {0,                             0},
    /* 66 */    {Keyboard::VK_BACKSP,           0},
    /* 67 */    {0,                             0},
    /* 68 */    {0,                             0},
    /* 69 */    {Keyboard::VK_KP1,              Keyboard::VK_END},
    /* 6A */    {0,                             0},
    /* 6B */    {Keyboard::VK_KP4,              Keyboard::VK_LEFT},
    /* 6C */    {Keyboard::VK_KP7,              Keyboard::VK_HOME},
    /* 6D */    {0,                             0},
    /* 6E */    {0,                             0},
    /* 6F */    {0,                             0},
    /* 70 */    {Keyboard::VK_KP0,              Keyboard::VK_INSERT},
    /* 71 */    {Keyboard::VK_KPDOT,            Keyboard::VK_DELETE},
    /* 72 */    {Keyboard::VK_KP2,              Keyboard::VK_DOWN},
    /* 73 */    {Keyboard::VK_KP5,              0},
    /* 74 */    {Keyboard::VK_KP6,              Keyboard::VK_RIGHT},
    /* 75 */    {Keyboard::VK_KP8,              Keyboard::VK_UP},
    /* 76 */    {Keyboard::VK_ESC,              0},
    /* 77 */    {Keyboard::VK_NUM,              0},
    /* 78 */    {Keyboard::VK_F11,              0},
    /* 79 */    {Keyboard::VK_KPADD,            0},
    /* 7A */    {Keyboard::VK_KP3,              Keyboard::VK_PGDOWN},
    /* 7B */    {Keyboard::VK_KPSUB,            0},
    /* 7C */    {Keyboard::VK_KPMUL,            0},
    /* 7D */    {Keyboard::VK_KP9,              Keyboard::VK_PGUP},
    /* 7E */    {Keyboard::VK_SCROLL,           0},
    /* 7F */    {0,                             0},
    /* 80 */    {0,                             0},
    /* 81 */    {0,                             0},
    /* 82 */    {0,                             0},
    /* 83 */    {Keyboard::VK_F7,               0},
};

bool Scancodes::get_keycode(bool &is_break, unsigned char &keycode, unsigned char scancode) {
    Entry *e;
    /* extended code-start? */
    if(scancode == 0xE0) {
        is_ext = true;
        return false;
    }
    /* break code? */
    if(scancode == 0xF0) {
        is_break_flag = true;
        return false;
    }

    /* get keycode */
    e = table + (scancode % 0x84);
    keycode = is_ext ? e->ext : e->def;
    is_break = is_break_flag;
    is_ext = false;
    is_break_flag = false;
    return true;
}
