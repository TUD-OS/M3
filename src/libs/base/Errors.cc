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
#include <base/Errors.h>

namespace m3 {

Errors::Code Errors::last;

static const char *errmsgs[] = {
    /*  0 */ "No error",
    /*  1 */ "Not enough credits",
    /*  2 */ "Not enough ringbuffer space",
    /*  3 */ "VPE gone",
    /*  4 */ "No mapping",
    /*  5 */ "Invalid arguments",
    /*  6 */ "Out of memory",
    /*  7 */ "No such file or directory",
    /*  8 */ "No permissions",
    /*  9 */ "Not supported",
    /* 10 */ "No free cores",
    /* 11 */ "Invalid ELF file",
    /* 12 */ "No space left",
    /* 13 */ "Object does already exist",
    /* 14 */ "Cross-filesystem link not possible",
    /* 15 */ "Directory not empty",
    /* 16 */ "Is a directory",
    /* 17 */ "Is no directory",
    /* 18 */ "Endpoint is invalid",
    /* 19 */ "Receive buffer gone",
    /* 20 */ "End of file",
    /* 21 */ "Messages are waiting to be handled",
};

const char *Errors::to_string(Code code) {
    size_t idx = code;
    if(idx < ARRAY_SIZE(errmsgs))
        return errmsgs[idx];
    return "Unknown error";
}

}
