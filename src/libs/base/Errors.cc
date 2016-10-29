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
    /*  7 */ "Invalid arguments",
    /*  8 */ "Out of memory",
    /*  9 */ "No such file or directory",
    /* 10 */ "No permissions",
    /* 11 */ "Not supported",
    /* 12 */ "No free PE",
    /* 13 */ "Invalid ELF file",
    /* 14 */ "No space left",
    /* 15 */ "Object does already exist",
    /* 16 */ "Cross-filesystem link not possible",
    /* 17 */ "Directory not empty",
    /* 18 */ "Is a directory",
    /* 19 */ "Is no directory",
    /* 20 */ "Endpoint is invalid",
    /* 21 */ "Receive buffer gone",
    /* 22 */ "End of file",
    /* 23 */ "Messages are waiting to be handled",
    /* 24 */ "Reply will be sent via upcall",
};

const char *Errors::to_string(Code code) {
    size_t idx = code;
    if(idx < ARRAY_SIZE(errmsgs))
        return errmsgs[idx];
    return "Unknown error";
}

}
