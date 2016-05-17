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
    /*  4 */ "Invalid arguments",
    /*  5 */ "Out of memory",
    /*  6 */ "No such file or directory",
    /*  7 */ "No permissions",
    /*  8 */ "Not supported",
    /*  9 */ "No free cores",
    /* 10 */ "Invalid ELF file",
    /* 11 */ "No space left",
    /* 12 */ "Object does already exist",
    /* 13 */ "Cross-filesystem link not possible",
    /* 14 */ "Directory not empty",
    /* 15 */ "Is a directory",
    /* 16 */ "Is no directory",
    /* 17 */ "Endpoint is invalid",
    /* 18 */ "Receive buffer gone",
    /* 19 */ "End of file",
    /* 20 */ "Messages are waiting to be handled",
};

const char *Errors::to_string(Code code) {
    size_t idx = code;
    if(idx < ARRAY_SIZE(errmsgs))
        return errmsgs[idx];
    return "Unknown error";
}

}
