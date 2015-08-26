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

#pragma once

#include <m3/Common.h>

#define MEMORY_CORE         0
#define KERNEL_CORE         0
#define APP_CORES           1
#define MAX_CORES           18
#define AVAIL_PES           (MAX_CORES - 1)
#define CAP_TOTAL           128
#define FS_IMG_OFFSET       0x0

#define HEAP_SIZE           0x100000
#define CHAN_COUNT          8

// TODO
#define CONF_LOCAL          0xF00000

namespace m3 {

class RecvGate;
extern RecvGate *def_rgate;

struct CoreConf {
    word_t coreid;
    /* padding */
    word_t : sizeof(word_t) * 8;
} PACKED;

static inline CoreConf *coreconf() {
    return reinterpret_cast<CoreConf*>(CONF_LOCAL);
}

static inline int coreid() {
    return coreconf()->coreid;
}

static inline RecvGate *def_rcvgate() {
    return def_rgate;
}

}
