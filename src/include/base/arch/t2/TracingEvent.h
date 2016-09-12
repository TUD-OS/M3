/*
 * Copyright (C) 2015, Matthias Lieber <matthias.lieber@tu-dresden.de>
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

#include <base/tracing/Config.h>
#include <stdint.h>

// ATTENTION: this file should not depend on any other include file,
// since we use it in the m3trace2otf converter

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

// reduce accuracy of timestamps by removing the least significant bits (relative timestamps only)
#define TIMESTAMP_SHIFT   2

/*

Event Types:
(64 bit)

                     most significant                               least significant
                     63-----|-------|-------|-------|-------|-------|-------|-------0

EVENT_TIMESTAMP      TTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
                     T Type 4 bits
                     C Absolute Timestamp 60 bits

EVENT_MSG_SEND/      TTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCSSSSSSSSSSSSSSSSRRRRRRAAAAAAAAAA
EVENT_MSG_RECV       T Type 4 bits
                     C Relative Timestamp 28 bits
                     S Msg size 16 bits
                     R Remote core 6 bits
                     A Msg Tag 10 bits

EVENT_MEM_READ/      TTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCSSSSSSSSSSSSSSSSRRRRRRDDDDDDDDDD
EVENT_MEM_WRITE      T Type 4 bits
                     C Relative Timestamp 28 bits
                     S Read/write size 16 bits
                     R Remote core 6 bits
                     D Operation duration 10 bits (if 0: followed by EVENT_MEM_FINISH)

EVENT_MEM_FINISH     TTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCC--------------------------------
                     T Type 4 bits
                     C Relative Timestamp 28 bits
                     - Unused

EVENT_FUNC_ENTER/    TTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
EVENT_FUNC_EXIT      T Type 4 bits
                     C Relative Timestamp 28 bits
                     I ID 32 bits (enter only, IDs defined in Event.h at compile time)

EVENT_UFUNC_ENTER/   TTTTCCCCCCCCCCCCCCCCCCCCCCCCCCCCNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
EVENT_UFUNC_EXIT     T Type 4 bits
                     C Relative Timestamp 28 bits
                     N Name (4 chars) 32 bits (enter only, names are created at run time)

*/

namespace m3 {

typedef uint64_t rec_t;

enum event_record_shifts : rec_t {
    REC_SHIFT_TYPE              = 60,
    REC_SHIFT_TIMESTAMP         = 32,
    REC_SHIFT_INIT_TIMESTAMP    = 0,
    REC_SHIFT_PAYLOAD           = 0,
    REC_SHIFT_MSG_SIZE          = 16,
    REC_SHIFT_MSG_REMOTE        = 10,
    REC_SHIFT_MSG_TAG           = 0,
    REC_SHIFT_MEM_SIZE          = 16,
    REC_SHIFT_MEM_REMOTE        = 10,
    REC_SHIFT_MEM_DURATION      = 0,
    REC_SHIFT_UFUNC_NAME        = 0,
    REC_SHIFT_FUNC_ID           = 0,
};

enum event_record_masks : rec_t {
    REC_MASK_TYPE               = (1 << 4) - 1,
    REC_MASK_TIMESTAMP          = (1 << 28) - 1,
    REC_MASK_INIT_TIMESTAMP     = ((rec_t)1 << 60) - 1,
    REC_MASK_PAYLOAD            = ((rec_t)1 << 32) - 1,
    REC_MASK_MSG_SIZE           = (1 << 16) - 1,
    REC_MASK_MSG_REMOTE         = (1 << 6) - 1,
    REC_MASK_MSG_TAG            = (1 << 10) - 1,
    REC_MASK_MEM_SIZE           = (1 << 16) - 1,
    REC_MASK_MEM_REMOTE         = (1 << 6) - 1,
    REC_MASK_MEM_DURATION       = (1 << 10) - 1,
    REC_MASK_UFUNC_NAME         = ((rec_t)1 << 32) - 1,
    REC_MASK_FUNC_ID            = ((rec_t)1 << 32) - 1,
};

// macros used as function IDs in event types EVENT_FUNC_ENTER and EVENT_FUNC_EXIT
// (only functions that do not use the above macros)
enum event_funcs {
    EVENT_FUNC_buffer_to_mem    = 0,
};

struct Event {
    // 64bit for a single record
    rec_t record;

    // dump the event (human readable)
    // friend OStream & operator<<(OStream &os, const Event &event);

    // get bit fields
    uint64_t type() const {
        return (record >> REC_SHIFT_TYPE) & REC_MASK_TYPE;
    }
    uint64_t timestamp() const {
        return (record >> REC_SHIFT_TIMESTAMP) & REC_MASK_TIMESTAMP;
    }
    uint64_t init_timestamp() const {
        return (record >> REC_SHIFT_INIT_TIMESTAMP) & REC_MASK_INIT_TIMESTAMP;
    }

#ifdef TRACE_HUMAN_READABLE
    uint64_t payload() const {
        return (record >> REC_SHIFT_PAYLOAD) & REC_MASK_PAYLOAD;
    }
    uint64_t msg_size() const {
        return (record >> REC_SHIFT_MSG_SIZE) & REC_MASK_MSG_SIZE;
    }
    uint64_t msg_remote() const {
        return (record >> REC_SHIFT_MSG_REMOTE) & REC_MASK_MSG_REMOTE;
    }
    uint64_t msg_tag() const {
        return (record >> REC_SHIFT_MSG_TAG) & REC_MASK_MSG_TAG;
    }
    uint64_t mem_size() const {
        return (record >> REC_SHIFT_MEM_SIZE) & REC_MASK_MEM_SIZE;
    }
    uint64_t mem_remote() const {
        return (record >> REC_SHIFT_MEM_REMOTE) & REC_MASK_MEM_REMOTE;
    }
    uint64_t mem_duration() const {
        return (record >> REC_SHIFT_MEM_DURATION) & REC_MASK_MEM_DURATION;
    }
    uint64_t func_id() const {
        return (record >> REC_SHIFT_FUNC_ID) & REC_MASK_FUNC_ID;
    }
    char* ufunc_name_str() const {
        static union {
            char chars[5];
            uint32_t i;
        } s;
        s.chars[4] = 0;
        s.i = func_id();
        return s.chars;
    }
#endif
};

}
