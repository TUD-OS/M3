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

#include <base/Common.h>

namespace m3 {

class Chars {
    Chars() = delete;

public:
    /**
     * @param c the character
     * @return true if its argument is a numeric digit or a letter of the alphabet.
     */
    static constexpr bool isalnum(int c) {
        return isalpha(c) || isdigit(c);
    }

    /**
     * @param c the character
     * @return true if its argument is a letter of the alphabet.
     */
    static constexpr bool isalpha(int c) {
        return islower(c) || isupper(c);
    }

    /**
     * @param c the character
     * @return true if its argument is ' ' or '\t'.
     */
    static constexpr bool isblank(int c) {
        return c == ' ' || c == '\t';
    }

    /**
     * @param c the character
     * @return true if its argument is a control-character ( < 0x20 )
     */
    static constexpr bool iscntrl(int c) {
        return c < 0x20 || c == 0xFF;
    }

    /**
     * @param c the character
     * @return true if its argument is a digit between 0 and 9.
     */
    static constexpr bool isdigit(int c) {
        return c >= '0' && c <= '9';
    }

    /**
     * @param c the character
     * @return true if its argument has a graphical representation.
     */
    static constexpr bool isgraph(int c) {
        return !iscntrl(c) && !isspace(c);
    }

    /**
     * @param c the character
     * @return true if its argument is a lowercase letter.
     */
    static constexpr bool islower(int c) {
        return c >= 'a' && c <= 'z';
    }

    /**
     * @param c the character
     * @return true if its argument is a printable character
     */
    static constexpr bool isprint(int c) {
        return !iscntrl(c);
    }

    /**
     * @param c the character
     * @return true if its argument is a punctuation character
     */
    static constexpr bool ispunct(int c) {
        return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') ||
            (c >= '{' && c <= '~');
    }

    /**
     * @param c the character
     * @return true if its argument is some sort of space (i.e. single space, tab,
     *  vertical tab, form feed, carriage return, or newline).
     */
    static constexpr bool isspace(int c) {
        return isblank(c) || c == '\v' || c == '\f' || c == '\r' || c == '\n';
    }

    /**
     * @param c the character
     * @return true if its argument is an uppercase letter.
     */
    static constexpr bool isupper(int c) {
        return c >= 'A' && c <= 'Z';
    }

    /**
     *
     * @param c the character
     * @return true if its argument is a hexadecimal digit
     */
    static constexpr bool isxdigit(int c) {
        return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    }

    /**
     * @param ch the char
     * @return the lowercase version of the character ch.
     */
    static constexpr int tolower(int ch) {
        return isupper(ch) ? (ch + ('a' - 'A')) : ch;
    }

    /**
     * @param ch the char
     * @return the uppercase version of the character ch.
     */
    static constexpr int toupper(int ch) {
        return islower(ch) ? (ch - ('a' - 'A')) : ch;
    }
};

}
