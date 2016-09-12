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

#include <base/tracing/Config.h>
#include <stdint.h>

namespace m3 {

enum event_types {
    EVENT_FUNC_ENTER            = 1,
    EVENT_FUNC_EXIT             = 2,
    EVENT_UFUNC_ENTER           = 3,
    EVENT_UFUNC_EXIT            = 4,
    EVENT_TIMESTAMP             = 5,
    EVENT_MSG_SEND              = 6,
    EVENT_MSG_RECV              = 7,
    EVENT_MEM_READ              = 8,
    EVENT_MEM_WRITE             = 9,
    EVENT_MEM_FINISH            = 10,
};

#if defined(TRACE_FUNCS_TO_STRING)
// only needed to extract the function name, not required while tracing is done

static const char* event_func_groups[] = {
    "Tracing",
    "Communication",
    "Syscall",
    "Service",
    "FS",
    "Main",
    "Kernel",
};

static const uint event_func_groups_size = sizeof(event_func_groups) / sizeof(char*);

struct event_func_names_and_group_struct {
    const char* name;
    uint group;
};

// this must match the EVENT_TRACER_* macros in Tracing.h
static const event_func_names_and_group_struct event_funcs[] = {
    { "buffer_to_mem",        0 },

    { "send_msg",             1 },
    { "send_vmsg",            1 },
    { "receive_msg",          1 },
    { "receive_vmsg",         1 },
    { "reply_msg",            1 },
    { "reply_vmsg",           1 },
    { "write_vmsg",           1 },
    { "read_sync",            1 },
    { "write_sync",           1 },
    { "cmpxchg_sync",         1 },

    { "Syscall_pagefault",    2 },
    { "Syscall_createsrv",    2 },
    { "Syscall_createsess",   2 },
    { "Syscall_createsessat", 2 },
    { "Syscall_creategate",   2 },
    { "Syscall_createvpe",    2 },
    { "Syscall_createmap",    2 },
    { "Syscall_attachrb",     2 },
    { "Syscall_detachrb",     2 },
    { "Syscall_exchange",     2 },
    { "Syscall_vpectrl",      2 },
    { "Syscall_reqmem",       2 },
    { "Syscall_derivemem",    2 },
    { "Syscall_delegate",     2 },
    { "Syscall_obtain",       2 },
    { "Syscall_delob_done",   2 },
    { "Syscall_activate",     2 },
    { "Syscall_activaterp",   2 },
    { "Syscall_revoke",       2 },
    { "Syscall_idle",         2 },
    { "Syscall_exit",         2 },

    { "Service_open",         3 },
    { "Service_obtain",       3 },
    { "Service_delegate",     3 },
    { "Service_close",        3 },
    { "Service_shutdown",     3 },
    { "Service_request",      3 },

    { "FS_open",              4 },
    { "FS_seek",              4 },
    { "FS_stat",              4 },
    { "FS_fstat",             4 },
    { "FS_mkdir",             4 },
    { "FS_rmdir",             4 },
    { "FS_link",              4 },
    { "FS_unlink",            4 },
    { "FS_close",             4 },
    { "FS_getlocs",           4 },

    { "Main",                 5 },
    { "Lambda",               5 },

    { "Kernel_Timeouts",      6 },
};

#endif

}
