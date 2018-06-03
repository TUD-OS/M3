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

#pragma once

#include <base/Common.h>

namespace RCTMux {

void print_num(uint64_t num, uint base);
void print(const char *str, size_t len);
void printf(const char *fmt, ...);

#define panic_if(cond, msg, ...) do { if((cond)) {                                              \
        printf("Panic: '" #cond "' at %s, %s() line %d: " msg, __FILE__, __FUNCTION__,          \
            __LINE__, ## __VA_ARGS__);                                                          \
        while(1)                                                                                \
            ;                                                                                   \
    } } while(0);

}
