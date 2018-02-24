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

#include "Print.h"

namespace RCTMux {

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

}
