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

#pragma once

namespace m3 {

/**
 * The error codes for M3
 */
struct Errors {
    enum Code {
        NONE,
        // DTU errors
        MISS_CREDITS,
        NO_RING_SPACE,
        VPE_GONE,
        PAGEFAULT,
        NO_MAPPING,
        INV_EP,
        ABORT,
        // SW errors
        INV_ARGS,
        OUT_OF_MEM,
        NO_SUCH_FILE,
        NO_PERM,
        NOT_SUP,
        NO_FREE_PE,
        INVALID_ELF,
        NO_SPACE,
        EXISTS,
        XFS_LINK,
        DIR_NOT_EMPTY,
        IS_DIR,
        IS_NO_DIR,
        EP_INVALID,
        RECV_GONE,
        END_OF_FILE,
        MSGS_WAITING,
        UPCALL_REPLY,
    };

    /**
     * @param code the error code
     * @return the statically allocated error message for <code>
     */
    static const char *to_string(Code code);

    /**
     * @return true if an error occurred
     */
    static bool occurred() {
        return last != NONE;
    }

    /**
     * @return the last error code
     */
    static Code last;
};

}
