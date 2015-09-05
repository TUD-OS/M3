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

#define MEMORY_CORE         8
#define KERNEL_CORE         0
#define APP_CORES           1
#define MAX_CORES           8
#define AVAIL_PES           (MAX_CORES - 1)
#define CAP_TOTAL           128
#define FS_IMG_OFFSET       0x0

#define HEAP_SIZE           0x10000
#define EP_COUNT            8

#define SPM_END             (4 * 1024 * 1024)
// leave one page for idle
#define STACK_TOP           (SPM_END - 0x1000)
#define STACK_BOTTOM        (STACK_TOP - 0x1000)

#define ARGV_SIZE           (0x1000 - 16)
#define ARGV_START          (STACK_BOTTOM - ARGV_SIZE)
#define ARGC_ADDR           (ARGV_START - 16)
#define ARGV_ADDR           (ARGV_START - 8)

#define BOOT_SP             (ARGV_START - 24)
#define BOOT_ENTRY          (BOOT_SP - 8)
#define BOOT_LAMBDA         (BOOT_ENTRY - 8)
#define BOOT_MOUNTLEN       (BOOT_LAMBDA - 8)
#define BOOT_MOUNTS         (BOOT_MOUNTLEN - 8)
#define BOOT_EPS            (BOOT_MOUNTS - 8)
#define BOOT_CAPS           (BOOT_EPS - 8)
#define BOOT_EXIT           (BOOT_CAPS - 8)

#define STATE_SIZE          0x100
#define STATE_SPACE         (BOOT_EXIT - STATE_SIZE)

#define DEF_RCVBUF_ORDER    8
#define DEF_RCVBUF_SIZE     (1 << DEF_RCVBUF_ORDER)
#define DEF_RCVBUF          (STATE_SPACE - DEF_RCVBUF_SIZE)

#define CONF_LOCAL          (DEF_RCVBUF - sizeof(word_t) * 2)

// end of space for runtime
#define RT_SPACE_END        CONF_LOCAL

#if defined(__cplusplus)

#   include <m3/Common.h>

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

#endif
