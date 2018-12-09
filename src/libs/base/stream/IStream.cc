/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/stream/IStream.h>
#include <base/util/Digits.h>
#include <base/util/Math.h>

namespace m3 {

static int get_digit_val(char c) {
    if(Chars::isdigit(c))
        return c - '0';
    return -1;
}

IStream & IStream::operator>>(String &str) {
    OStringStream os;
    char c;
    skip_whitespace();
    while(1) {
        c = read();
        if(bad() || c == '\n')
            break;
        os << c;
    }
    str.reset(os.str(), os.length());
    return *this;
}

size_t IStream::getline(char *buffer, size_t max, char delim) {
    assert(max >= 1);
    char *begin = buffer;
    char *end = buffer + max - 1;
    do {
        *buffer = read();
        if(bad() || *buffer == delim)
            break;
        buffer++;
    }
    while(buffer < end);
    *buffer = '\0';
    return static_cast<size_t>(buffer - begin);
}

IStream &IStream::read_float(float &f) {
    bool neg = false;
    int cnt = 0;
    f = 0;
    skip_whitespace();

    /* handle +/- */
    char c = read();
    if(c == '-' || c == '+') {
        neg = c == '-';
        c = read();
        cnt++;
    }

    /* in front of "." */
    while(good()) {
        int val = get_digit_val(c);
        if(val == -1)
            break;
        f = f * 10 + val;
        c = read();
        cnt++;
    }

    /* after "." */
    if(c == '.') {
        cnt++;
        uint mul = 10;
        while(good()) {
            c = read();
            int val = get_digit_val(c);
            if(val == -1)
                break;
            f += (float)val / mul;
            mul *= 10;
        }
    }

    /* handle exponent */
    if(c == 'e' || c == 'E') {
        c = read();
        cnt++;
        bool negexp = c == '-';
        if(c == '-' || c == '+')
            c = read();

        int expo = 0;
        while(good()) {
            int val = get_digit_val(c);
            if(val == -1)
                break;
            expo = expo * 10 + val;
            c = read();
        }

        while(expo-- > 0)
            f *= negexp ? 0.1f : 10.f;
    }

    if(cnt > 0)
        putback(c);
    if(neg)
        f = -f;
    return *this;
}

}
