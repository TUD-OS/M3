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

#pragma once

#include <base/Config.h>
#include <base/arch/t2/TracingEvent.h>
#include <base/tracing/Event.h>
#include <base/tracing/Config.h>

#define KERNEL_CORE                             4
#define FIRST_PE_ID                             4

namespace m3 {

class Tracing {
public:
    Tracing();

    static inline Tracing &get() {
        return _inst;
    }

    inline void event_msg_send(uchar remotecore, size_t length, uint16_t tag) {
        if(trace_enabled && trace_sendrecv && trace_core[remotecore] && trace_core[env()->coreid]) {
            trace_enabled = false;
            record_event_msg(EVENT_MSG_SEND, remotecore, length, tag);
            trace_enabled = true;
        }
    }

    inline void event_msg_recv(uchar remotecore, size_t length, uint16_t tag) {
        if(trace_enabled && trace_sendrecv && trace_core[remotecore] && trace_core[env()->coreid]) {
            trace_enabled = false;
            record_event_msg(EVENT_MSG_RECV, remotecore, length, tag);
            trace_enabled = true;
        }
    }

    inline void event_mem_read(uchar remotecore, size_t length) {
        if(trace_enabled && trace_read && trace_core[remotecore] && trace_core[env()->coreid]) {
            trace_enabled = false;
            record_event_mem(EVENT_MEM_READ, remotecore, length);
            trace_enabled = true;
        }
    }

    inline void event_mem_write(uchar remotecore, size_t length) {
        if(trace_enabled && trace_write && trace_core[remotecore] && trace_core[env()->coreid]) {
            trace_enabled = false;
            record_event_mem(EVENT_MEM_WRITE, remotecore, length);
            trace_enabled = true;
        }
    }

    inline void event_mem_finish() {
        if(trace_enabled && last_mem_event) {
            trace_enabled = false;
            record_event_mem_finish();
            trace_enabled = true;
        }
    }

    inline void event_ufunc_enter(const char name[5]) {
        if(trace_enabled && trace_ufunc && trace_core[env()->coreid]) {
            trace_enabled = false;
            record_event_func(EVENT_UFUNC_ENTER, *((uint32_t*)name));
            trace_enabled = true;
        }
    }

    inline void event_ufunc_exit() {
        if(trace_enabled && trace_ufunc && trace_core[env()->coreid]) {
            trace_enabled = false;
            record_event_func_exit(EVENT_UFUNC_EXIT);
            trace_enabled = true;
        }
    }

    inline void event_func_enter(uint32_t id) {
        if(trace_enabled && trace_func && trace_core[env()->coreid]) {
            trace_enabled = false;
            record_event_func(EVENT_FUNC_ENTER, id);
            trace_enabled = true;
        }
    }

    inline void event_func_exit() {
        if(trace_enabled && trace_func && trace_core[env()->coreid]) {
            trace_enabled = false;
            record_event_func_exit(EVENT_FUNC_EXIT);
            trace_enabled = true;
        }
    }

    /**
     * flush local buffer to Mem
     * expects reinit() before next event (events will be flushed instantly between flush() and reinit())
     */
    void flush();

    /**
     * flush local buffer to Mem if buffer is >85% full
     * no reinit expected, i.e. can be called any time when it's a good time for a buffer flush
     */
    void flush_light();

    /**
     * reinitialize, get buffer pos in Mem from Mem
     * do not call from kernel
     */
    void reinit();

    /**
     * kernel must call this to initialize tracing before
     * others call reinit()
     */
    void init_kernel();

    /**
     * dump trace to stdout (dumb!)
     */
    void trace_dump();

private:
    void record_event_msg(uint8_t type, uchar remotecore, size_t length, uint16_t tag);
    void record_event_mem(uint8_t type, uchar remotecore, size_t length);
    void record_event_mem_finish();
    void record_event_timestamp();
    void record_event_timestamp(cycles_t);
    void record_event_func(uint8_t type, uint32_t id);
    void record_event_func_exit(uint8_t type);
    inline void advance_event_buffer();
    void send_event_buffer_to_mem();
    void reset();
    static void mem_write(size_t addr, const void * buf, size_t size);
    static void mem_read(size_t addr, void * buf, size_t size);
    inline uintptr_t membuf_of(uint core) {
        return core * TRACE_MEMBUF_SIZE + TRACE_MEMBUF_ADDR;
    }
    inline void handle_large_delta(cycles_t now) {
        if(REC_MASK_TIMESTAMP << TIMESTAMP_SHIFT < (now - last_timestamp))
            record_event_timestamp(now);
    }
    inline void handle_zero_delta(UNUSED cycles_t *now) {
        // if we use the Core Manager for cycle counting, we will never see a zero delta
#if CCOUNT_CORE != CM_CORE
        if(*now == last_timestamp)
            *now += 1 << TIMESTAMP_SHIFT;
#endif
    }

    static Tracing _inst;

    /// defines which PEs to trace
#if defined(__t2__)
    //                               v Mem   v Kernel  v User PEs
    const bool trace_core[12] = {0,0,1,    0,1,        1,1,1,1,1,1,1};
#endif

    /// defines which data transfers to trace
    const bool trace_read = true;
    const bool trace_write = true;
    const bool trace_sendrecv = true;
    const bool trace_func = true;
    const bool trace_ufunc = true;

    /// saves last timestamp to record only deltas
    cycles_t last_timestamp;

    /// last mem event
    Event* last_mem_event;

    /// local event buffer
    Event eventbuf[TRACE_EVENTBUF_SIZE];
    Event* next_event;
    Event* eventbuf_half = eventbuf + TRACE_EVENTBUF_SIZE/2;
    Event* eventbuf_end = eventbuf + TRACE_EVENTBUF_SIZE;

    /// Mem event buffer
    uintptr_t membuf_start;
    uintptr_t membuf_cur;

    /// disable tracing for all stuff that happens inside the Tracing class
    /// to avoid recursion
    bool trace_enabled;

    /// if we are in this state, every new event will be followed by a flush to RAM
    bool between_flush_and_reinit;

    /// number of events seen so far, counts also those events dropped due to buffer overflow
    uint event_counter;
};

}
