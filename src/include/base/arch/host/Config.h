/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#define PE_COUNT             18
#define CAP_TOTAL           128

#define FS_MAX_SIZE         (256 * 1024 * 1024)
#define FS_IMG_OFFSET       0

#define PAGE_BITS           0
#define PAGE_SIZE           1   // shouldn't be 0 because of the alignment in reqmem
#define PAGE_MASK           0

#define STACK_SIZE          0x1000

#define RECVBUF_SPACE       1   // no limit here
#define RECVBUF_SIZE        16384U
#define RECVBUF_SIZE_SPM    16384U

#define MAX_RB_SIZE         32

#define RCTMUX_ENTRY        0   // unused
#define RCTMUX_YIELD        0   // unused
#define RCTMUX_FLAGS        0   // unused

// this has to be large enough for forwarded memory reads
#define SYSC_RBUF_ORDER     9
#define SYSC_RBUF_SIZE      (1 << SYSC_RBUF_ORDER)

#define UPCALL_RBUF_ORDER   8
#define UPCALL_RBUF_SIZE    (1 << UPCALL_RBUF_ORDER)

#define DEF_RBUF_ORDER      8
#define DEF_RBUF_SIZE       (1 << DEF_RBUF_ORDER)

#define MEMCAP_END          (~0UL)
