/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/stream/Serial.h>
#include <m3/Config.h>

#if defined(__host__)
#   define __log_lock()   m3::Config::get().log_lock()
#   define __log_unlock() m3::Config::get().log_unlock()
#else
#   define __log_lock()
#   define __log_unlock()
#endif

#define LOG(lvl, expr)                                               \
    do {                                                             \
        if(m3::Log::level & (m3::Log::lvl)) {                        \
            __log_lock();                                            \
            m3::Serial::get() << expr << '\n';                       \
            __log_unlock();                                          \
        }                                                            \
    }                                                                \
    while(0)

#define PANIC(expr) do {                                             \
        __log_lock();                                                \
        m3::Serial::get() << expr << "\n";                           \
        __log_unlock();                                              \
        exit(1);                                                     \
    }                                                                \
    while(0)

namespace m3 {

class Log {
    Log() = delete;

public:
    enum Level {
        DEF         = 1 << 0,
        SYSC        = 1 << 1,
        KSYSC       = 1 << 2,
        VPES        = 1 << 3,
        DTU         = 1 << 4,
        DTUERR      = 1 << 5,
        IPC         = 1 << 6,
        SHM         = 1 << 7,
        IRQS        = 1 << 8,
        KEYB        = 1 << 9,
        FSPROXY     = 1 << 10,
        PRELOAD     = 1 << 11,
        EXIT        = 1 << 12,
        FS          = 1 << 13,
        FS_DBG      = 1 << 14,
        CAPS        = 1 << 15,
        KERR        = 1 << 16,
    };

#if defined(__t2__) || defined(__t3__)
    static const int level = DEF | KERR | KSYSC | VPES | DTUERR | FS;
#else
    static const int level = DEF | KERR | KSYSC | VPES | DTUERR | FS;
#endif
};

}
