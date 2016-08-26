/**
 * Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
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

#ifndef NDEBUG

#if defined(__t3__)
EXTERN_C void __assert(const char *, const char *, unsigned int, const char *) {
    while(1);
}
#else
EXTERN_C void __assert(const char *, const char *, int) {
    while(1);
}

EXTERN_C void __assert_fail(const char *, const char *, unsigned int, const char *) throw() {
    while(1);
}
#endif /* defined(__t3__) */

#endif /* !NDEBUG */
