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

#define MAX_CORES           8
#define AVAIL_PES           (MAX_CORES - 1)

#define SLOT_NO             4
#define CHAN_COUNT          8
#define CAP_TOTAL           128
#define FS_IMG_OFFSET       0x1000000
#define CODE_BASE_ADDR      0x60010000

#define DRAM_CCOUNT         0x100000

#define STACK_TOP           0x60010000              // actually, we only control that on the chip
// give the stack 4K
#define DRAM_VEND           0x6000F000

#define HEAP_SIZE           0x7000                  // not the actual size, but the maximum

#define RECV_BUF_MSGSIZE    64
#define RECV_BUF_LOCAL      (DRAM_VEND - (CHAN_COUNT * RECV_BUF_MSGSIZE * MAX_CORES))
#define RECV_BUF_GLOBAL     (RECV_BUF_LOCAL - DRAM_VOFFSET)

#define ARGC_ADDR           (RECV_BUF_LOCAL - 8)
#define ARGV_ADDR           (ARGC_ADDR - 8)
#define ARGV_SIZE           0x400
#define ARGV_START          (ARGV_ADDR - ARGV_SIZE)

#define SERIAL_ACK          (ARGV_START - 8)
#define SERIAL_BUFSIZE      0x100
#define SERIAL_BUF          (SERIAL_ACK - SERIAL_BUFSIZE)

#define BOOT_SP             (SERIAL_BUF - 8)
#define BOOT_ENTRY          (BOOT_SP - 8)
#define BOOT_LAMBDA         (BOOT_ENTRY - 8)
#define BOOT_MOUNTLEN       (BOOT_LAMBDA - 8)
#define BOOT_MOUNTS         (BOOT_MOUNTLEN - 8)
#define BOOT_CHANS          (BOOT_MOUNTS - 8)
#define BOOT_CAPS           (BOOT_CHANS - 8)
#define BOOT_EXIT           (BOOT_CAPS - 8)

#define STATE_SIZE          0x100
#define STATE_SPACE         (BOOT_EXIT - STATE_SIZE)

// this is currently used for the data of the boot-code (it can't overlap with the data of
// normal programs)
#define BOOT_DATA_SIZE      0x160
#define BOOT_DATA           (STATE_SPACE - BOOT_DATA_SIZE)

#ifdef __cplusplus
#   include <m3/Common.h>
#   ifdef __t2_sim__
#       include <m3/arch/t2-sim/Addr.h>
#   else
#       include <m3/arch/t2-chip/Addr.h>
#   endif
#   include <m3/BitField.h>

#   define IRQ_ADDR_EXTERN  0x20020
#   define IRQ_ADDR_INTERN  CM_SO_PE_CLEAR_IRQ

namespace m3 {

class RecvGate;

struct ChanConf {
    uchar dstcore;
    uchar dstchan;
    // padding
    ushort : sizeof(ushort) * 8;
    word_t credits;
    label_t label;
    // padding
    word_t : sizeof(word_t) * 8;
} PACKED;

struct CoreConf {
    word_t coreid;
    // padding
    word_t : sizeof(word_t) * 8;
    ChanConf chans[CHAN_COUNT];
} PACKED;

// align it properly
#   define CONF_LOCAL          (BOOT_DATA - ((sizeof(m3::CoreConf) + 8 - 1) & ~(8 - 1)))
#   define CONF_GLOBAL         (CONF_LOCAL - DRAM_VOFFSET)

// end of space for runtime
#   define RT_SPACE_END        CONF_LOCAL

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
