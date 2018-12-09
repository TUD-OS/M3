/*
 * Copyright (C) 2016, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/Common.h>
#include <base/Config.h>

namespace m3 {

struct EPConf {
    uchar valid;
    uchar dstcore;
    uchar dstep;
    // padding
    uchar : sizeof(uchar) * 8;
    word_t credits;
    label_t label;
    // padding
    word_t : sizeof(word_t) * 8;
} PACKED;

static inline EPConf *eps() {
    // don't require this to be true for host programs (mkm3fs, ...)
#if !defined(__tools__)
    static_assert(sizeof(EPConf) == EP_SIZE, "EP_SIZE wrong");
#endif

    return reinterpret_cast<EPConf*>(EPS_START);
}

}
