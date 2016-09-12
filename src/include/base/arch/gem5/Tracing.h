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
#include <base/DTU.h>

namespace m3 {

class Tracing {
public:
    static inline Tracing &get() {
        return _inst;
    }

    inline void event_msg_send(uchar, size_t, uint16_t) {
    }

    inline void event_msg_recv(uchar, size_t, uint16_t) {
    }

    inline void event_mem_read(uchar, size_t) {
    }

    inline void event_mem_write(uchar, size_t) {
    }

    inline void event_mem_finish() {
    }

    inline void event_ufunc_enter(const char name[5]) {
        DTU::get().debug_msg(
            ((uint64_t)EVENT_UFUNC_ENTER << 48) |
            ((uint64_t)name[0] << 24) |
            ((uint64_t)name[1] << 16) |
            ((uint64_t)name[2] <<  8) |
            ((uint64_t)name[3] <<  0)
        );
    }

    inline void event_ufunc_exit() {
        DTU::get().debug_msg((uint64_t)EVENT_UFUNC_EXIT << 48);
    }

    inline void event_func_enter(uint32_t id) {
        DTU::get().debug_msg(((uint64_t)EVENT_FUNC_ENTER << 48) | id);
    }

    inline void event_func_exit() {
        DTU::get().debug_msg((uint64_t)EVENT_FUNC_EXIT << 48);
    }

    void flush() {
    }
    void flush_light() {
    }
    void reinit() {
    }
    void init_kernel() {
    }
    void trace_dump() {
    }

private:

    static Tracing _inst;
};

}
