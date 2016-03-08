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

#define MEMORY_CORE         0
#define MAX_CORES           18
#define AVAIL_PES           (MAX_CORES - 1)
#define PE_MASK             0xFFFFFFFF

#define EP_COUNT            7
#define CAP_TOTAL           256
#define FS_IMG_OFFSET       0x1000000
#define CODE_BASE_ADDR      0x60800000

#define PAGE_BITS           0
#define PAGE_SIZE           0
#define PAGE_MASK           0

// leave the first 64 MiB for the filesystem
#define DRAM_OFFSET         (64 * 1024 * 1024)
#define DRAM_SIZE           (64 * 1024 * 1024)

#define STACK_TOP           0x60800000              // used for stack-copying
#define STACK_SIZE          0x1000
// give the stack 4K and a bit of space for idle
#define DMEM_VEND           0x607FD000

#define INIT_HEAP_SIZE      0                       // not used
#define HEAP_SIZE           0x200000                // not the actual size, but the maximum

#define RT_START            0                       // not used

#define ARGC_ADDR           (DMEM_VEND - 8)
#define ARGV_ADDR           (ARGC_ADDR - 8)
#define ARGV_SIZE           0x100
#define ARGV_START          (ARGV_ADDR - ARGV_SIZE)

#define SERIAL_ACK          (ARGV_START - 8)
#define SERIAL_INWAIT       (SERIAL_ACK - 8)
#define SERIAL_BUFSIZE      0x100
#define SERIAL_BUF          (SERIAL_INWAIT - SERIAL_BUFSIZE)

#define BOOT_SP             (SERIAL_BUF - 8)
#define BOOT_ENTRY          (BOOT_SP - 8)
#define BOOT_LAMBDA         (BOOT_ENTRY - 8)
#define BOOT_PAGER_SESS     (BOOT_LAMBDA - 8)
#define BOOT_PAGER_GATE     (BOOT_PAGER_SESS - 8)
#define BOOT_MOUNTLEN       (BOOT_PAGER_GATE - 8)
#define BOOT_MOUNTS         (BOOT_MOUNTLEN - 8)
#define BOOT_EPS            (BOOT_MOUNTS - 8)
#define BOOT_CAPS           (BOOT_EPS - 8)
#define BOOT_EXIT           (BOOT_CAPS - 8)

#define STATE_SIZE          0x100
#define STATE_SPACE         (BOOT_EXIT - STATE_SIZE)

#define DEF_RCVBUF_ORDER    8
#define DEF_RCVBUF_SIZE     (1 << DEF_RCVBUF_ORDER)
#define DEF_RCVBUF          (STATE_SPACE - DEF_RCVBUF_SIZE)

#define CONF_LOCAL          (DEF_RCVBUF - 8)
#define CONF_GLOBAL         (CONF_LOCAL - DRAM_VOFFSET)

// end of space for runtime
#define RT_SPACE_END        CONF_LOCAL

#ifdef __cplusplus
#   include <m3/Common.h>
#   include <m3/arch/t3/Addr.h>
#   include <m3/BitField.h>

#   define IRQ_ADDR_EXTERN  \
        (IDMA_CONFIG_ADDR + (DEBUG_CMD << IDMA_CMD_POS) + (0x2 << IDMA_SLOT_TRG_ID_POS))
#   define IRQ_ADDR_INTERN  \
        (PE_IDMA_CONFIG_ADDRESS + (DEBUG_CMD << PE_IDMA_CMD_POS) + (0x2 << PE_IDMA_SLOT_TRG_ID_POS))

namespace m3 {

class RecvGate;

struct CoreConf {
    word_t coreid;
    /* padding */
    word_t : sizeof(word_t) * 8;
} PACKED;

extern RecvGate *def_rgate;

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
