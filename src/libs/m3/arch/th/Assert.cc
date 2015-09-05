/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/Common.h>
#include <m3/Log.h>

#ifndef NDEBUG
EXTERN_C void __assert(const char *failedexpr, const char *file, unsigned int line, const char *func) throw() {
    m3::Serial::get() << "assertion \"" << failedexpr << "\" failed in " << func << " in "
                  << file << ":" << line << "\n";
    abort();
    /* NOTREACHED */
}
#endif
