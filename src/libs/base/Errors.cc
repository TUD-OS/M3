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
    /*  6 */ "Abort",
    /*  7 */ "VPE id invalid",
    /*  8 */ "Invalid arguments",
    /*  9 */ "Out of memory",
    /* 10 */ "No such file or directory",
    /* 11 */ "No permissions",
    /* 12 */ "Not supported",
    /* 13 */ "No free cores",
    /* 14 */ "Invalid ELF file",
    /* 15 */ "No space left",
    /* 16 */ "Object does already exist",
    /* 17 */ "Cross-filesystem link not possible",
    /* 18 */ "Directory not empty",
    /* 19 */ "Is a directory",
    /* 20 */ "Is no directory",
    /* 21 */ "Endpoint is invalid",
    /* 22 */ "Receive buffer gone",
    /* 23 */ "End of file",
    /* 24 */ "Messages are waiting to be handled",
    /* 25 */ "Reply will be sent via upcall",
};

const char *Errors::to_string(Code code) {
    size_t idx = code;
    if(idx < ARRAY_SIZE(errmsgs))
        return errmsgs[idx];
    return "Unknown error";
}

}
