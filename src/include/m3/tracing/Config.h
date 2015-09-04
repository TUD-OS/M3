/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

// this does currently only work on the T2 chip
#if defined(__t2__)

// enable/disable tracing
//#define TRACE_ENABLED

// enable/disable decoding of events
#define TRACE_HUMAN_READABLE

// trace buffer in DRAM
// (we need these defines for consts2ini even if TRACING is not defined)
#define TRACE_MEMBUF_SIZE       (8 * 32 * 1024)     // bytes per PE
#define TRACE_MEMBUF_ADDR       0x200000            // address of memory event buffer (all PEs)

// convert messsage address (without offset) to a small as possible integer
// (by dividing by RECV_BUF_MSGSIZE=64)
#define TRACE_ADDR2TAG_SHIFT    6

// trace buffer in scratchpad
#define TRACE_EVENTBUF_SIZE     256                 // number of events, each 8 byte, per PE

#endif
