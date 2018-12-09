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

#if defined(__cplusplus)
#   include <base/arch/t3/Addr.h>
#endif

#define MAX_CORES           18

#define EP_COUNT            7
#define CAP_TOTAL           256
#define FS_IMG_OFFSET       0x1000000
#define CODE_BASE_ADDR      0x60800000

#define IRQ_ADDR_EXTERN  \
        (IDMA_CONFIG_ADDR + (DEBUG_CMD << IDMA_CMD_POS) + (0x2 << IDMA_SLOT_TRG_ID_POS))
#define IRQ_ADDR_INTERN  \
        (PE_IDMA_CONFIG_ADDRESS + (DEBUG_CMD << PE_IDMA_CMD_POS) + (0x2 << PE_IDMA_SLOT_TRG_ID_POS))

// defined in linker script
#define RCTMUX_FLAGS_LOCAL  0x6081FFF8
#define RCTMUX_FLAGS_GLOBAL (RCTMUX_FLAGS_LOCAL - DRAM_VOFFSET)

#define PAGE_BITS           0
#define PAGE_SIZE           0
#define PAGE_MASK           0

#define STACK_TOP           0x60800000              // used for stack-copying
#define STACK_SIZE          0x1000

// give the stack 4K and a bit of space for idle
#define DMEM_VEND           0x607FD000

#define APP_HEAP_SIZE       0                       // not used
#define HEAP_SIZE           0x200000                // not the actual size, but the maximum

#define SPMEM_LAYOUT_LOCAL  (DMEM_VEND - 32)
#define SPMEM_LAYOUT_GLOBAL (SPMEM_LAYOUT_LOCAL - DRAM_VOFFSET)

#define RT_SIZE             0x400
#define RT_START            (SPMEM_LAYOUT_LOCAL - RT_SIZE)
#define RT_END              SPMEM_LAYOUT_LOCAL

#define RECVBUF_SPACE       1                       // no limit here

#define DEF_RCVBUF_ORDER    8
#define DEF_RCVBUF_SIZE     (1 << DEF_RCVBUF_ORDER)
#define DEF_RCVBUF          (RT_END - DEF_RCVBUF_SIZE)

#define MEMCAP_END          0xFFFFFFFF
