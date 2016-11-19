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

#include <base/Common.h>
#include <base/stream/Serial.h>

#include <assert.h>
#include <stdlib.h>

#ifndef NDEBUG

void __assert_failed(const char *expr, const char *file, const char *func, int line) {
    m3::Serial::get() << "assertion \"" << expr << "\" failed in " << func << " in "
                      << file << ":" << line << "\n";
    abort();
    /* NOTREACHED */
}

#endif
