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
#define KERNEL_CORE         0
#define MAX_CORES           18
#define CAP_TOTAL           128
#define FS_IMG_OFFSET       0x0

#define PAGE_BITS           0
#define PAGE_SIZE           0
#define PAGE_MASK           0

// leave the first 64 MiB for the filesystem
#define DRAM_OFFSET         0
#define DRAM_SIZE           (512 * 1024 * 1024)

#define STACK_SIZE          0x1000

#define RECVBUF_SPACE       1                       // no limit here

#define MEMCAP_END          0xFFFFFFFFFFFFFFFF
