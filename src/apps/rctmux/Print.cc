/**
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <stdarg.h>

#include "Print.h"

namespace RCTMux {

static size_t strlen(const char *s) {
    size_t len = 0;
    while(*s++)
        len++;
    return len;
}

static size_t print_num_rec(char *buf, size_t pos, uint64_t num, uint base) {
    size_t p = pos;
    if(num > base)
        p = print_num_rec(buf, pos - 1, num / base, base);
    buf[pos] = "0123456789abcdef"[num % base];
    return p;
}

void print_num(uint64_t num, uint base) {
    char buf[16];
    size_t first = print_num_rec(buf, ARRAY_SIZE(buf) - 1, num, base);
    print(&buf[first], ARRAY_SIZE(buf) - first);
}

void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    while(*fmt) {
        if(*fmt != '%')
            print(fmt++, 1);
        else {
            fmt++;
            switch(*fmt) {
                case 'u':
                case 'x': {
                    unsigned long num = va_arg(ap, unsigned long);
                    print_num(num, *fmt == 'u' ? 10 : 16);
                    break;
                }

                case 's': {
                    const char *s = va_arg(ap, const char*);
                    print(s, strlen(s));
                    break;
                }
            }
            fmt++;
        }
    }
    va_end(ap);
}

}
