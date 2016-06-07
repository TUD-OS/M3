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

#if defined(__cplusplus)
#   include <base/arch/t2/Addr.h>
#endif

#define MAX_CORES           12
#define PE_MASK             0xFE2       // CM, PE1, ..., PE7

#define SLOT_NO             4
#define EP_COUNT            8
#define CAP_TOTAL           128
#define FS_IMG_OFFSET       0x1000000
#define CODE_BASE_ADDR      0x60010000

#define PAGE_BITS           0
#define PAGE_SIZE           0
#define PAGE_MASK           0

#define IRQ_ADDR_EXTERN     0x20020
#define IRQ_ADDR_INTERN     CM_SO_PE_CLEAR_IRQ

// leave the first 64 MiB for the filesystem
#define DRAM_OFFSET         (64 * 1024 * 1024)
#define DRAM_SIZE           (64 * 1024 * 1024)

#define DRAM_FILE_AREA      0x0FFF00
#define DRAM_FILE_AREA_LEN  0x100
#define DRAM_BLOCKNO        0
#define DRAM_FILESIZE       8
#define DRAM_FILENAME       16
#define DRAM_FILENAME_LEN   (DRAM_FILE_AREA_LEN - DRAM_FILENAME)

#define DRAM_CCOUNT         0x100000
#define CM_CCOUNT           0xFFF0
#define CM_CCOUNT_AT_CM     0x6000FFF0

// set to 0 to use the app-core and DRAM
#define CCOUNT_CM           0

#if CCOUNT_CM == 1
#   define CCOUNT_CORE         CM_CORE
#   define CCOUNT_ADDR         CM_CCOUNT
#else
#   define CCOUNT_CORE         MEMORY_CORE
#   define CCOUNT_ADDR         DRAM_CCOUNT
#endif

#define STACK_TOP           0x60010000              // actually, we only control that on the chip
#define STACK_SIZE          0x1000
// give the stack 4K
#define DMEM_VEND           0x6000F000

#define INIT_HEAP_SIZE      0                       // not used
#define HEAP_SIZE           0x7000                  // not the actual size, but the maximum

#define RECV_BUF_MSGSIZE    64
#define RECV_BUF_LOCAL      (DMEM_VEND - (EP_COUNT * RECV_BUF_MSGSIZE * MAX_CORES))
#define RECV_BUF_GLOBAL     (RECV_BUF_LOCAL - DRAM_VOFFSET)

#define SERIAL_ACK          (RECV_BUF_LOCAL - 8)
#define SERIAL_INWAIT       (SERIAL_ACK - 8)
#define SERIAL_BUFSIZE      0x100
#define SERIAL_BUF          (SERIAL_INWAIT - SERIAL_BUFSIZE)

// this is currently used for the data of the boot-code (it can't overlap with the data of
// normal programs)
#define BOOT_DATA_SIZE      0x160
#define BOOT_DATA           (SERIAL_BUF - BOOT_DATA_SIZE)

#define EP_SIZE             16
#define EPS_SIZE            (EP_COUNT * EP_SIZE)
#define EPS_START           (BOOT_DATA - EPS_SIZE)

#define RT_SIZE             0x400
#define RT_START            (EPS_START - RT_SIZE)
#define RT_END              EPS_START

#define RECVBUF_SPACE       1                       // no limit here

// actually, it does not really matter here what the values are
#define DEF_RCVBUF_ORDER    4
#define DEF_RCVBUF_SIZE     (1 << DEF_RCVBUF_ORDER)
#define DEF_RCVBUF          0

#define MEMCAP_END          0xFFFFFFFF
