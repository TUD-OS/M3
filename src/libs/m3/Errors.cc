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

#include <m3/Common.h>
#include <m3/Errors.h>

namespace m3 {

Errors::Code Errors::last;

static const char *errmsgs[] = {
    /*  0 */ "No error",
    /*  1 */ "Invalid arguments",
    /*  2 */ "Out of memory",
    /*  3 */ "No such file or directory",
    /*  4 */ "No permissions",
    /*  5 */ "Not supported",
    /*  6 */ "No free cores",
    /*  7 */ "Invalid ELF file",
    /*  8 */ "No space left",
    /*  9 */ "Object does already exist",
    /* 10 */ "Object gone",
    /* 11 */ "Cross-filesystem link not possible",
    /* 12 */ "Directory not empty",
    /* 13 */ "Is a directory",
    /* 13 */ "Is no directory",
};

const char *Errors::to_string(Code code) {
    size_t idx = -code;
    if(idx < ARRAY_SIZE(errmsgs))
        return errmsgs[idx];
    return "Unknown error";
}

}
