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
#include <base/tracing/Event.h>
#include <base/tracing/Config.h>

// macros for each M3 function to trace
// (EVENT_TRACER will be deactivated if TRACING is not defined)
#define EVENT_TRACER_send_msg()             EVENT_TRACER(1)
#define EVENT_TRACER_send_vmsg()            EVENT_TRACER(2)
#define EVENT_TRACER_receive_msg()          EVENT_TRACER(3)
#define EVENT_TRACER_receive_vmsg()         EVENT_TRACER(4)
#define EVENT_TRACER_reply_msg()            EVENT_TRACER(5)
#define EVENT_TRACER_reply_vmsg()           EVENT_TRACER(6)
#define EVENT_TRACER_write_vmsg()           EVENT_TRACER(7)
#define EVENT_TRACER_read_sync()            EVENT_TRACER(8)
#define EVENT_TRACER_write_sync()           EVENT_TRACER(9)
#define EVENT_TRACER_cmpxchg_sync()         EVENT_TRACER(10)
// syscalls
#define EVENT_TRACER_Syscall_pagefault()    EVENT_TRACER(11);
#define EVENT_TRACER_Syscall_createsrv()    EVENT_TRACER(12);
#define EVENT_TRACER_Syscall_createsess()   EVENT_TRACER(13);
#define EVENT_TRACER_Syscall_createsessat() EVENT_TRACER(14);
#define EVENT_TRACER_Syscall_creatergate()  EVENT_TRACER(15);
#define EVENT_TRACER_Syscall_creategate()   EVENT_TRACER(16);
#define EVENT_TRACER_Syscall_createvpe()    EVENT_TRACER(17);
#define EVENT_TRACER_Syscall_createmap()    EVENT_TRACER(18);
#define EVENT_TRACER_Syscall_exchange()     EVENT_TRACER(19);
#define EVENT_TRACER_Syscall_vpectrl()      EVENT_TRACER(20);
#define EVENT_TRACER_Syscall_reqmem()       EVENT_TRACER(21);
#define EVENT_TRACER_Syscall_derivemem()    EVENT_TRACER(22);
#define EVENT_TRACER_Syscall_delegate()     EVENT_TRACER(23);
#define EVENT_TRACER_Syscall_obtain()       EVENT_TRACER(24);
#define EVENT_TRACER_Syscall_delob_done()   EVENT_TRACER(25);
#define EVENT_TRACER_Syscall_activate()     EVENT_TRACER(26);
#define EVENT_TRACER_Syscall_forwardmsg()   EVENT_TRACER(27);
#define EVENT_TRACER_Syscall_forwardmem()   EVENT_TRACER(28);
#define EVENT_TRACER_Syscall_forwardreply() EVENT_TRACER(29);
#define EVENT_TRACER_Syscall_revoke()       EVENT_TRACER(30);
#define EVENT_TRACER_Syscall_idle()         EVENT_TRACER(31);
#define EVENT_TRACER_Syscall_noop()         EVENT_TRACER(32);
#define EVENT_TRACER_Syscall_exit()         EVENT_TRACER(33);
// service commands
#define EVENT_TRACER_Service_open()         EVENT_TRACER(34);
#define EVENT_TRACER_Service_obtain()       EVENT_TRACER(35);
#define EVENT_TRACER_Service_delegate()     EVENT_TRACER(36);
#define EVENT_TRACER_Service_close()        EVENT_TRACER(37);
#define EVENT_TRACER_Service_shutdown()     EVENT_TRACER(38);
#define EVENT_TRACER_Service_request()      EVENT_TRACER(39);
// fs commands
#define EVENT_TRACER_FS_open()              EVENT_TRACER(40);
#define EVENT_TRACER_FS_seek()              EVENT_TRACER(41);
#define EVENT_TRACER_FS_stat()              EVENT_TRACER(42);
#define EVENT_TRACER_FS_fstat()             EVENT_TRACER(43);
#define EVENT_TRACER_FS_mkdir()             EVENT_TRACER(44);
#define EVENT_TRACER_FS_rmdir()             EVENT_TRACER(45);
#define EVENT_TRACER_FS_link()              EVENT_TRACER(46);
#define EVENT_TRACER_FS_unlink()            EVENT_TRACER(47);
#define EVENT_TRACER_FS_close()             EVENT_TRACER(48);
#define EVENT_TRACER_FS_getlocs()           EVENT_TRACER(49);
// main
#define EVENT_TRACER_Main()                 EVENT_TRACER(50);
#define EVENT_TRACER_Lambda()               EVENT_TRACER(51);
// kernel
#define EVENT_TRACER_Kernel_Timeouts()      EVENT_TRACER(52);

#if defined(TRACE_ENABLED)

/// interface macros - user applications
/// - function name (user functions) must be a fixed size char[5] (4 letters + '\0')
///
/// enter a user function, exit will be called if objects comes out of scope
#define EVENT_USER_TRACER(name)                 m3::EventUserTracer my_user_tracer__(name);
/// enter a user function
#define EVENT_USER_ENTER(name)                  m3::Tracing::get().event_ufunc_enter(name);
/// exit a user function
#define EVENT_USER_EXIT()                       m3::Tracing::get().event_ufunc_exit();

/// interface macros - M3 internal
/// - function id (sys functions) must be predefined in Event.h
///
/// enter a sys function, exit will be called if objects comes out of scope
#define EVENT_TRACER(id)                        m3::EventTracer my_tracer__(id);
/// enter a sys function
//#define EVENT_ENTER(id)                         m3::Tracing::get().event_func_enter(id);
/// exit a sys function
//#define EVENT_EXIT()                            m3::Tracing::get().event_func_exit();
/// memory read/write is finished
#define EVENT_TRACE_MEM_FINISH()                m3::Tracing::get().event_mem_finish();
/// memory read
#define EVENT_TRACE_MEM_READ(pe, len)           m3::Tracing::get().event_mem_read(pe, len);
/// memory write
#define EVENT_TRACE_MEM_WRITE(pe, len)          m3::Tracing::get().event_mem_write(pe, len);
/// message send
#define EVENT_TRACE_MSG_SEND(pe, len, tag)      m3::Tracing::get().event_msg_send(pe, len, tag);
/// message receive
#define EVENT_TRACE_MSG_RECV(pe, len, tag)      m3::Tracing::get().event_msg_recv(pe, len, tag);
///
/// initialize at kernel
#define EVENT_TRACE_INIT_KERNEL()               m3::Tracing::get().init_kernel();
/// (re)initialize
#define EVENT_TRACE_REINIT()                    m3::Tracing::get().reinit();
/// flush here if buffer is >80% full, optional
#define EVENT_TRACE_FLUSH_LIGHT()               m3::Tracing::get().flush_light();
/// flush here, reinit necessary before next event
#define EVENT_TRACE_FLUSH()                     m3::Tracing::get().flush();
/// dump trace to stdout (kernel only)
#define EVENT_TRACE_DUMP()                      m3::Tracing::get().trace_dump();

#if defined(__t2__)
#   include <base/arch/t2/Tracing.h>
#elif defined(__gem5__)
#   include <base/arch/gem5/Tracing.h>
#endif

namespace m3 {

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
#define EVENT_TRACE_MEM_READ(pe, length)
#define EVENT_TRACE_MEM_WRITE(pe, length)
#define EVENT_TRACE_MSG_SEND(pe, length, tag)
#define EVENT_TRACE_MSG_RECV(pe, length, tag)
#define EVENT_TRACE_INIT_KERNEL()
#define EVENT_TRACE_REINIT()
#define EVENT_TRACE_FLUSH_LIGHT()
#define EVENT_TRACE_FLUSH()
#define EVENT_TRACE_DUMP()

#endif
