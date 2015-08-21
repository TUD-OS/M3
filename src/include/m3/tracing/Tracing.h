/*
 * Copyright (C) 2015, Matthias Lieber <matthias.lieber@tu-dresden.de>
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

#include <m3/Config.h>
#include <m3/tracing/Event.h>
#include <m3/tracing/Config.h>

// macros for each M3 function to trace
// (EVENT_TRACER will be deactivated if TRACING is not defined)
#define EVENT_TRACER_send_msg()             EVENT_TRACER(1)
#define EVENT_TRACER_send_vmsg()            EVENT_TRACER(2)
#define EVENT_TRACER_receive_msg()          EVENT_TRACER(3)
#define EVENT_TRACER_receive_vmsg()         EVENT_TRACER(4)
#define EVENT_TRACER_send_receive_msg()     EVENT_TRACER(5)
#define EVENT_TRACER_send_receive_vmsg()    EVENT_TRACER(6)
#define EVENT_TRACER_reply_msg()            EVENT_TRACER(7)
#define EVENT_TRACER_reply_msg_on()         EVENT_TRACER(8)
#define EVENT_TRACER_reply_vmsg()           EVENT_TRACER(9)
#define EVENT_TRACER_reply_vmsg_on()        EVENT_TRACER(10)
#define EVENT_TRACER_write_vmsg()           EVENT_TRACER(11)
#define EVENT_TRACER_KWorkLoop_run()        EVENT_TRACER(12)
#define EVENT_TRACER_lambda_main()          EVENT_TRACER(13)
#define EVENT_TRACER_read_sync()            EVENT_TRACER(14)
#define EVENT_TRACER_write_sync()           EVENT_TRACER(15)
#define EVENT_TRACER_cmpxchg_sync()         EVENT_TRACER(16)
#define EVENT_TRACER_handle_message()       EVENT_TRACER(17)
// syscalls
#define EVENT_TRACER_Syscall_createsrv()    EVENT_TRACER(18);
#define EVENT_TRACER_Syscall_createsess()   EVENT_TRACER(19);
#define EVENT_TRACER_Syscall_creategate()   EVENT_TRACER(20);
#define EVENT_TRACER_Syscall_createvpe()    EVENT_TRACER(21);
#define EVENT_TRACER_Syscall_attachrb()     EVENT_TRACER(22);
#define EVENT_TRACER_Syscall_detachrb()     EVENT_TRACER(23);
#define EVENT_TRACER_Syscall_exchange()     EVENT_TRACER(24);
#define EVENT_TRACER_Syscall_vpectrl()      EVENT_TRACER(25);
#define EVENT_TRACER_Syscall_reqmem()       EVENT_TRACER(26);
#define EVENT_TRACER_Syscall_derivemem()    EVENT_TRACER(27);
#define EVENT_TRACER_Syscall_delegate()     EVENT_TRACER(28);
#define EVENT_TRACER_Syscall_obtain()       EVENT_TRACER(29);
#define EVENT_TRACER_Syscall_delob_done()   EVENT_TRACER(30);
#define EVENT_TRACER_Syscall_activate()     EVENT_TRACER(31);
#define EVENT_TRACER_Syscall_revoke()       EVENT_TRACER(32);
#define EVENT_TRACER_Syscall_exit()         EVENT_TRACER(33);
// service commands
#define EVENT_TRACER_Service_open()         EVENT_TRACER(34);
#define EVENT_TRACER_Service_obtain()       EVENT_TRACER(35);
#define EVENT_TRACER_Service_delegate()     EVENT_TRACER(36);
#define EVENT_TRACER_Service_close()        EVENT_TRACER(37);
#define EVENT_TRACER_Service_shutdown()     EVENT_TRACER(38);

// here some functions can be filtered at compile time by redefining the macros
#undef  EVENT_TRACER_read_sync
#define EVENT_TRACER_read_sync()
#undef  EVENT_TRACER_write_sync
#define EVENT_TRACER_write_sync()
#undef  EVENT_TRACER_reply_msg
#define EVENT_TRACER_reply_msg()
#undef  EVENT_TRACER_reply_msg_on
#define EVENT_TRACER_reply_msg_on()
#undef  EVENT_TRACER_reply_vmsg
#define EVENT_TRACER_reply_vmsg()
#undef  EVENT_TRACER_reply_vmsg_on
#define EVENT_TRACER_reply_vmsg_on()
#undef  EVENT_TRACER_handle_message
#define EVENT_TRACER_handle_message()

#if defined(TRACE_ENABLED)

/// interface macros - user applications
/// - function name (user functions) must be a fixed size char[5] (4 letters + '\0')
///
/// enter a user function, exit will be called if objects comes out of scope
#define EVENT_USER_TRACER(name)                 EventUserTracer my_user_tracer__(name);
/// enter a user function
#define EVENT_USER_ENTER(name)                  Tracing::get().event_ufunc_enter(name);
/// exit a user function
#define EVENT_USER_EXIT()                       Tracing::get().event_ufunc_exit();

/// interface macros - M3 internal
/// - function id (sys functions) must be predefined in Event.h
///
/// enter a sys function, exit will be called if objects comes out of scope
#define EVENT_TRACER(id)                        EventTracer my_tracer__(id);
/// enter a sys function
//#define EVENT_ENTER(id)                         Tracing::get().event_func_enter(id);
/// exit a sys function
//#define EVENT_EXIT()                            Tracing::get().event_func_exit();
/// memory read/write is finished
#define EVENT_TRACE_MEM_FINISH()                Tracing::get().event_mem_finish();
/// memory read
#define EVENT_TRACE_MEM_READ(core, len)         Tracing::get().event_mem_read(core, len);
/// memory write
#define EVENT_TRACE_MEM_WRITE(core, len)        Tracing::get().event_mem_write(core, len);
/// message send
#define EVENT_TRACE_MSG_SEND(core, len, tag)    Tracing::get().event_msg_send(core, len, tag);
/// message receive
#define EVENT_TRACE_MSG_RECV(core, len, tag)    Tracing::get().event_msg_recv(core, len, tag);
///
/// initialize at kernel
#define EVENT_TRACE_INIT_KERNEL()               Tracing::get().init_kernel();
/// (re)initialize
#define EVENT_TRACE_REINIT()                    Tracing::get().reinit();
/// flush here if buffer is >80% full, optional
#define EVENT_TRACE_FLUSH_LIGHT()               Tracing::get().flush_light();
/// flush here, reinit necessary before next event
#define EVENT_TRACE_FLUSH()                     Tracing::get().flush();
/// dump trace to stdout (kernel only)
#define EVENT_TRACE_DUMP()                      Tracing::get().trace_dump();

namespace m3 {

class Tracing {
public:
    Tracing();

    static inline Tracing &get() {
        return _inst;
    }

    inline void event_msg_send(uchar remotecore, size_t length, uint16_t tag) {
        if(trace_enabled && trace_sendrecv && trace_core[remotecore] && trace_core[coreid()]) {
            trace_enabled = false;
            record_event_msg(EVENT_MSG_SEND, remotecore, length, tag);
            trace_enabled = true;
        }
    }

    inline void event_msg_recv(uchar remotecore, size_t length, uint16_t tag) {
        if(trace_enabled && trace_sendrecv && trace_core[remotecore] && trace_core[coreid()]) {
            trace_enabled = false;
            record_event_msg(EVENT_MSG_RECV, remotecore, length, tag);
            trace_enabled = true;
        }
    }

    inline void event_mem_read(uchar remotecore, size_t length) {
        if(trace_enabled && trace_read && trace_core[remotecore] && trace_core[coreid()]) {
            trace_enabled = false;
            record_event_mem(EVENT_MEM_READ, remotecore, length);
            trace_enabled = true;
        }
    }

    inline void event_mem_write(uchar remotecore, size_t length) {
        if(trace_enabled && trace_write && trace_core[remotecore] && trace_core[coreid()]) {
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
        if(trace_enabled && trace_ufunc && trace_core[coreid()]) {
            trace_enabled = false;
            record_event_func(EVENT_UFUNC_ENTER, *((uint32_t*)name));
            trace_enabled = true;
        }
    }

    inline void event_ufunc_exit() {
        if(trace_enabled && trace_ufunc && trace_core[coreid()]) {
            trace_enabled = false;
            record_event_func_exit(EVENT_UFUNC_EXIT);
            trace_enabled = true;
        }
    }

    inline void event_func_enter(uint32_t id) {
        if(trace_enabled && trace_func && trace_core[coreid()]) {
            trace_enabled = false;
            record_event_func(EVENT_FUNC_ENTER, id);
            trace_enabled = true;
        }
    }

    inline void event_func_exit() {
        if(trace_enabled && trace_func && trace_core[coreid()]) {
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
#else
    bool trace_core[FIRST_PE_ID+MAX_CORES];
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

class EventUserTracer {
public:
    inline EventUserTracer(const char name[5]) {
        Tracing::get().event_ufunc_enter(name);
    }
    inline ~EventUserTracer() {
        Tracing::get().event_ufunc_exit();
    }
};

class EventTracer {
public:
    inline EventTracer(uint32_t id) {
        Tracing::get().event_func_enter(id);
    }
    inline ~EventTracer() {
        Tracing::get().event_func_exit();
    }
};

}

#else

// deactivate the interface macros
#define EVENT_USER_TRACER(name)
#define EVENT_USER_ENTER(name)
#define EVENT_USER_EXIT()
#define EVENT_TRACER(id)
//#define EVENT_ENTER(id) Tracing::get().event_func_enter(id);
//#define EVENT_EXIT() Tracing::get().event_func_exit();
#define EVENT_TRACE_MEM_FINISH()
#define EVENT_TRACE_MEM_READ(remotecore, length)
#define EVENT_TRACE_MEM_WRITE(remotecore, length)
#define EVENT_TRACE_MSG_SEND(remotecore, length, tag)
#define EVENT_TRACE_MSG_RECV(remotecore, length, tag)
#define EVENT_TRACE_INIT_KERNEL()
#define EVENT_TRACE_REINIT()
#define EVENT_TRACE_FLUSH_LIGHT()
#define EVENT_TRACE_FLUSH()
#define EVENT_TRACE_DUMP()

#endif
