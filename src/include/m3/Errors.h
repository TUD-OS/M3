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
        NO_ERROR        = 0,
        INV_ARGS        = -1,
        OUT_OF_MEM      = -2,
        NO_SUCH_FILE    = -3,
        NO_PERM         = -4,
        NOT_SUP         = -5,
        NO_FREE_CORE    = -6,
        INVALID_ELF     = -7,
        NO_SPACE        = -8,
        EXISTS          = -9,
        GONE            = -10,
        XFS_LINK        = -11,
        DIR_NOT_EMPTY   = -12,
        IS_DIR          = -13,
        IS_NO_DIR       = -14,
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
        return last != NO_ERROR;
    }

    /**
     * @return the last error code
     */
    static Code last;
};

}
