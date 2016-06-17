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
    /*  4 */ "Pagefault",
    /*  5 */ "No mapping",
    /*  6 */ "Invalid arguments",
    /*  7 */ "Out of memory",
    /*  8 */ "No such file or directory",
    /*  9 */ "No permissions",
    /* 10 */ "Not supported",
    /* 11 */ "No free cores",
    /* 12 */ "Invalid ELF file",
    /* 13 */ "No space left",
    /* 14 */ "Object does already exist",
    /* 15 */ "Cross-filesystem link not possible",
    /* 16 */ "Directory not empty",
    /* 17 */ "Is a directory",
    /* 18 */ "Is no directory",
    /* 19 */ "Endpoint is invalid",
    /* 20 */ "Receive buffer gone",
    /* 21 */ "End of file",
    /* 22 */ "Messages are waiting to be handled",
};

const char *Errors::to_string(Code code) {
    size_t idx = code;
    if(idx < ARRAY_SIZE(errmsgs))
        return errmsgs[idx];
    return "Unknown error";
}

}
